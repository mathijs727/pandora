#pragma once
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/ray.h"
#include "pandora/traversal/pauseable_bvh.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include "simd/intrinsics.h"
#include "simd/simd4.h"
#include <embree3/rtcore.h>
#include <iostream>
#include <limits>
#include <nmmintrin.h> // popcnt
#include <optional>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_vector.h>
#include <tuple>

namespace pandora {

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
class PauseableBVH4 : PauseableBVH<LeafObj, HitRayState, AnyHitRayState> {
public:
    PauseableBVH4(gsl::span<LeafObj> object);
    PauseableBVH4(PauseableBVH4&&) = default;
    ~PauseableBVH4() = default;

    size_t sizeBytes() const override final;

    //std::optional<bool> intersect(Ray& ray, RayHit& hitInfo, const HitRayState& userState) const override final;
    //std::optional<bool> intersect(Ray& ray, RayHit& hitInfo, const HitRayState& userState, PauseableBVHInsertHandle handle) const override final;

    std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState) const override final;
    std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState, PauseableBVHInsertHandle handle) const override final;

    std::optional<bool> intersectAny(Ray& ray, const AnyHitRayState& userState) const override final;
    std::optional<bool> intersectAny(Ray& ray, const AnyHitRayState& userState, PauseableBVHInsertHandle handle) const override final;

    gsl::span<LeafObj*> leafs() { return m_leafs; }

private:
    template <bool AnyHit, typename UserState>
    std::optional<bool> intersectT(Ray& ray, SurfaceInteraction& hitInfo, const UserState& userState, PauseableBVHInsertHandle insertInfo) const;

    struct TestBVHData {
        int numPrimitives = 0;
        int maxDepth = 0;
        std::array<int, 5> numChildrenHistogram = { 0, 0, 0, 0, 0 };
    };
    struct BVHNode;
    void testBVH() const;
    void testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const;

    // Tuple returning node pointer and isLeaf
    struct ConstructionInnerNode;
    uint32_t generateFinalBVH(ConstructionInnerNode* node, gsl::span<LeafObj> leafs);
    void generateFinalBVHRecurse(ConstructionInnerNode* node, uint32_t parentHandle, uint32_t depth, uint32_t outHandle, gsl::span<LeafObj> leafs);

    std::vector<LeafObj*> collectLeafs() const;
    void collectLeafsRecurse(const BVHNode& node, std::vector<LeafObj*>& outLeafObjects) const;

    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    struct ConstructionNode {
        virtual void dummy() const {};
    };
    struct ConstructionInnerNode : public ConstructionNode {
        eastl::fixed_vector<Bounds, 4> childrenBounds;
        eastl::fixed_vector<ConstructionNode*, 4> children;
    };
    struct ConstructionLeafNode : public ConstructionNode {
        uint32_t leafID { 0 };
    };

    struct alignas(16) BVHNode {
        simd::vec4_f32 minX;
        simd::vec4_f32 minY;
        simd::vec4_f32 minZ;
        simd::vec4_f32 maxX;
        simd::vec4_f32 maxY;
        simd::vec4_f32 maxZ;

        uint32_t firstChildHandle;
        uint32_t firstLeafHandle;

        uint32_t parentHandle;

        uint32_t validMask : 4;
        uint32_t leafMask : 4;
        uint32_t depth : 8;

        uint32_t numChildren() const;
        bool isLeaf(unsigned childIdx) const;
        bool isInnerNode(unsigned childIdx) const;

        uint32_t getInnerChildHandle(unsigned childIdx) const;
        uint32_t getLeafChildHandle(unsigned childIdx) const;
    };
    static_assert(sizeof(BVHNode) <= 128);

    //ContiguousAllocatorTS<typename PauseableBVH4::BVHNode> m_innerNodeAllocator;
    //ContiguousAllocatorTS<LeafObj> m_leafAllocator;
    //gsl::span<LeafObj> m_tmpConstructionLeafs;
    std::vector<BVHNode> m_bvhNodes;
    std::vector<LeafObj> m_leafs;

    uint32_t m_rootHandle;
};

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::PauseableBVH4(gsl::span<LeafObj> leafs)
{
    // Store in the class so that the (static) construction functions can access the objects
    //  and move them into the BVH leafs
    //m_tmpConstructionLeafs = leafs;

    // Create a representatin of the leafs that Embree will understand
    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.reserve(leafs.size());
    for (unsigned i = 0; i < static_cast<unsigned>(leafs.size()); i++) {
        auto bounds = leafs[i].getBounds();

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.geomID = 0;
        primitive.primID = i;
        embreeBuildPrimitives.push_back(primitive);
    }

    const auto embreeErrorFunc = [](void* userPtr, const RTCError code, const char* str) {
        switch (code) {
        case RTC_ERROR_NONE:
            std::cout << "RTC_ERROR_NONE";
            break;
        case RTC_ERROR_UNKNOWN:
            std::cout << "RTC_ERROR_UNKNOWN";
            break;
        case RTC_ERROR_INVALID_ARGUMENT:
            std::cout << "RTC_ERROR_INVALID_ARGUMENT";
            break;
        case RTC_ERROR_INVALID_OPERATION:
            std::cout << "RTC_ERROR_INVALID_OPERATION";
            break;
        case RTC_ERROR_OUT_OF_MEMORY:
            std::cout << "RTC_ERROR_OUT_OF_MEMORY";
            break;
        case RTC_ERROR_UNSUPPORTED_CPU:
            std::cout << "RTC_ERROR_UNSUPPORTED_CPU";
            break;
        case RTC_ERROR_CANCELLED:
            std::cout << "RTC_ERROR_CANCELLED";
            break;
        }

        std::cout << ": " << str << std::endl;
    };

    // Build the BVH using the Embree BVH builder API
    RTCDevice device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
    RTCBVH bvh = rtcNewBVH(device);

    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
    arguments.maxBranchingFactor = 4;
    arguments.minLeafSize = 1;
    arguments.maxLeafSize = 1;
    arguments.bvh = bvh;
    arguments.primitives = embreeBuildPrimitives.data();
    arguments.primitiveCount = embreeBuildPrimitives.size();
    arguments.primitiveArrayCapacity = embreeBuildPrimitives.capacity();
    arguments.createNode = innerNodeCreate;
    arguments.setNodeChildren = innerNodeSetChildren;
    arguments.setNodeBounds = innerNodeSetBounds;
    arguments.createLeaf = leafCreate;
    arguments.userPtr = this;
    ConstructionNode* constructionRoot = reinterpret_cast<ConstructionNode*>(rtcBuildBVH(&arguments));
    if (auto* constructionRootInner = dynamic_cast<ConstructionInnerNode*>(constructionRoot)) {
        m_rootHandle = generateFinalBVH(constructionRootInner, leafs);
    } else {
        ALWAYS_ASSERT(false, "Leaf node cannot be the root of PauseableBVH4");
    }
    //setNodeDepth(m_innerNodeAllocator.get(nodeHandle), 0);

    // Releases Embree memory (including the temporary BVH)
    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

    // Do this after we call m_leafAllocator.compact() which will move the leafs to a new block of memory
    //m_leafs = collectLeafs();

    testBVH();
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline size_t PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::sizeBytes() const
{
    size_t size = sizeof(decltype(*this));
    size += m_bvhNodes.size() * sizeof(BVHNode);
    size += m_leafs.size() * sizeof(LeafObj*);
    return size;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline std::optional<bool> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState) const
{
    return intersect(ray, si, userState, { m_rootHandle, 0xFFFFFFFFFFFFFFFF });
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline std::optional<bool> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::intersect(Ray& ray, SurfaceInteraction& si, const HitRayState& userState, PauseableBVHInsertHandle insertInfo) const
{
    return intersectT<false, HitRayState>(ray, si, userState, insertInfo);
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline std::optional<bool> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::intersectAny(Ray& ray, const AnyHitRayState& userState) const
{
    SurfaceInteraction dummySI {};
    return intersectT<true>(ray, dummySI, userState, { m_rootHandle, 0xFFFFFFFFFFFFFFFF });
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline std::optional<bool> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::intersectAny(Ray& ray, const AnyHitRayState& userState, PauseableBVHInsertHandle insertInfo) const
{
    SurfaceInteraction dummySI {};
    return intersectT<true>(ray, dummySI, userState, insertInfo);
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
template <bool AnyHit, typename UserState>
inline std::optional<bool> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::intersectT(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle insertInfo) const
{
    struct SIMDRay {
        simd::vec4_f32 originX;
        simd::vec4_f32 originY;
        simd::vec4_f32 originZ;

        simd::vec4_f32 invDirectionX;
        simd::vec4_f32 invDirectionY;
        simd::vec4_f32 invDirectionZ;

        simd::vec4_f32 tnear;
        simd::vec4_f32 tfar;
    };
    SIMDRay simdRay;
    simdRay.originX = simd::vec4_f32(ray.origin.x);
    simdRay.originY = simd::vec4_f32(ray.origin.y);
    simdRay.originZ = simd::vec4_f32(ray.origin.z);
    simdRay.invDirectionX = simd::vec4_f32(ray.direction.x == 0.0f ? 0.0f : 1.0f / ray.direction.x);
    simdRay.invDirectionY = simd::vec4_f32(ray.direction.y == 0.0f ? 0.0f : 1.0f / ray.direction.y);
    simdRay.invDirectionZ = simd::vec4_f32(ray.direction.z == 0.0f ? 0.0f : 1.0f / ray.direction.z);
    simdRay.tnear = simd::vec4_f32(ray.tnear);
    simdRay.tfar = simd::vec4_f32(ray.tfar);

    // Stack
    bool hit = false;
    auto [nodeHandle, stack] = insertInfo;
    const BVHNode* node = &m_bvhNodes[nodeHandle];
    while (true) {
        // Get traversal bits at the current depth
        int bitPos = 4 * node->depth;
        uint64_t interestBitMask = (stack >> bitPos) & 0b1111;
        if (interestBitMask != 0) {
            // clang-format off
            const simd::mask4 interestMask(
                interestBitMask & 0x1,
                interestBitMask & 0x2,
                interestBitMask & 0x4,
                interestBitMask & 0x8);
            // clang-format on

            // Find the nearest intersection of hte ray and the child boxes
            const simd::vec4_f32 tx1 = (node->minX - simdRay.originX) * simdRay.invDirectionX;
            const simd::vec4_f32 tx2 = (node->maxX - simdRay.originX) * simdRay.invDirectionX;
            const simd::vec4_f32 ty1 = (node->minY - simdRay.originY) * simdRay.invDirectionY;
            const simd::vec4_f32 ty2 = (node->maxY - simdRay.originY) * simdRay.invDirectionY;
            const simd::vec4_f32 tz1 = (node->minZ - simdRay.originZ) * simdRay.invDirectionZ;
            const simd::vec4_f32 tz2 = (node->maxZ - simdRay.originZ) * simdRay.invDirectionZ;
            const simd::vec4_f32 txMin = simd::min(tx1, tx2);
            const simd::vec4_f32 tyMin = simd::min(ty1, ty2);
            const simd::vec4_f32 tzMin = simd::min(tz1, tz2);
            const simd::vec4_f32 txMax = simd::max(tx1, tx2);
            const simd::vec4_f32 tyMax = simd::max(ty1, ty2);
            const simd::vec4_f32 tzMax = simd::max(tz1, tz2);
            const simd::vec4_f32 tmin = simd::max(simdRay.tnear, simd::max(txMin, simd::max(tyMin, tzMin)));
            const simd::vec4_f32 tmax = simd::min(simdRay.tfar, simd::min(txMax, simd::min(tyMax, tzMax)));
            const simd::mask4 hitMask = tmin <= tmax;

            const simd::mask4 toVisitMask = hitMask && interestMask;
            if (toVisitMask.any()) {
                // Find nearest active child for this ray
                unsigned childIndex;
                if constexpr (AnyHit) {
                    childIndex = simd::bitScan32(static_cast<uint32_t>(toVisitMask.bitMask()));
                } else {
                    const simd::vec4_f32 inf4(std::numeric_limits<float>::max());
                    const simd::vec4_f32 maskedDistances = simd::blend(inf4, tmin, toVisitMask);
                    childIndex = maskedDistances.horizontalMinIndex();
                }
                assert(childIndex <= 4);

                uint64_t toVisitBitMask = toVisitMask.bitMask();
                toVisitBitMask ^= (1llu << childIndex); // Set the bit of the child we are visiting to 0
                stack = stack ^ (interestBitMask << bitPos); // Set the bits in the stack corresponding to the current node to 0
                stack = stack | (toVisitBitMask << bitPos); // And replace them by the new mask

                if (node->isInnerNode(childIndex)) {
                    //nodeHandle = node->childrenHandles[childIndex];
                    nodeHandle = node->getInnerChildHandle(childIndex);
                    node = &m_bvhNodes[nodeHandle];
                } else {
                    // Reached leaf
                    //auto handle = node->childrenHandles[childIndex];
                    auto handle = node->getLeafChildHandle(childIndex);
                    const auto& leaf = m_leafs[handle];
                    if constexpr (AnyHit) {
                        auto optResult = leaf.intersectAny(ray, userState, { nodeHandle, stack });
                        if (!optResult)
                            return {};

                        if (*optResult)
                            return true;
                    } else {
                        auto optResult = leaf.intersect(ray, si, userState, { nodeHandle, stack });
                        if (!optResult)
                            return {}; // Ray was paused

                        if (*optResult) {
                            hit = true;
                            simdRay.tfar = simd::vec4_f32(ray.tfar);
                        }
                    }
                }

                continue;
            }
        }

        // No children left to visit; find the first ancestor that has work left

        // Set all bits after bitPos to 1
        uint64_t oldStack = stack;
        stack = stack | (0xFFFFFFFFFFFFFFFF << bitPos);
        if (stack == 0xFFFFFFFFFFFFFFFF)
            break;

        int prevDepth = node->depth;
        nodeHandle = node->parentHandle;
        node = &m_bvhNodes[nodeHandle];
        assert(node->depth == prevDepth - 1);
    }

    return hit;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::testBVH() const
{
    TestBVHData results;
    testBVHRecurse(&m_bvhNodes[m_rootHandle], 0, results);

    std::cout << std::endl;
    std::cout << " <<< PauseableBVH4 Build results >>> " << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Primitives reached: " << results.numPrimitives << std::endl;
    std::cout << "Max depth:          " << results.maxDepth << std::endl;
    std::cout << "\nChild count histogram:\n";
    for (size_t i = 0; i < results.numChildrenHistogram.size(); i++)
        std::cout << i << ": " << results.numChildrenHistogram[i] << std::endl;
    std::cout << std::endl;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const
{
    unsigned numChildrenReference = 0;
    for (unsigned childIdx = 0; childIdx < 4; childIdx++) {
        if (node->isLeaf(childIdx)) {
            out.numPrimitives++;
            numChildrenReference++;
            assert(node->getLeafChildHandle(childIdx) < m_leafs.size());
        } else if (node->isInnerNode(childIdx)) {
            testBVHRecurse(&m_bvhNodes[node->getInnerChildHandle(childIdx)], depth + 1, out);
            numChildrenReference++;
        }
    }

    unsigned numChildren = node->numChildren();
    assert(numChildren == numChildrenReference);

    assert(depth == node->depth);
    out.maxDepth = std::max(out.maxDepth, depth);
    out.numChildrenHistogram[numChildren]++;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline uint32_t PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::generateFinalBVH(ConstructionInnerNode* node, gsl::span<LeafObj> leafs)
{
    // Allocate root node
    uint32_t handle { 0 };
    m_bvhNodes.emplace_back();

    // Fill root node recursively
    generateFinalBVHRecurse(node, 0, 0, handle, leafs);

    return handle;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::generateFinalBVHRecurse(ConstructionInnerNode* constructionInnerNode, uint32_t parentHandle, uint32_t depth, uint32_t outHandle, gsl::span<LeafObj> leafs)
{
    assert(constructionInnerNode->children.size() == constructionInnerNode->childrenBounds.size());
    BVHNode& outNode = m_bvhNodes[outHandle];

    // Copy the bounding boxes
    std::array<float, 4> minX, minY, minZ, maxX, maxY, maxZ;
    for (size_t i = 0; i < constructionInnerNode->children.size(); i++) {
        const auto& childBounds = constructionInnerNode->childrenBounds[i];
        minX[i] = childBounds.min.x;
        minY[i] = childBounds.min.y;
        minZ[i] = childBounds.min.z;
        maxX[i] = childBounds.max.x;
        maxY[i] = childBounds.max.y;
        maxZ[i] = childBounds.max.z;
    }
    for (size_t i = constructionInnerNode->children.size(); i < 4; i++) {
        minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
    }
    outNode.minX.load(minX);
    outNode.minY.load(minY);
    outNode.minZ.load(minZ);
    outNode.maxX.load(maxX);
    outNode.maxY.load(maxY);
    outNode.maxZ.load(maxZ);

    outNode.depth = depth;
    outNode.parentHandle = parentHandle;

    // Compute the valid & leaf masks and collect a typed list of the children
    eastl::fixed_vector<ConstructionInnerNode*, 4> constructionInnerChildPointers;
    eastl::fixed_vector<ConstructionLeafNode*, 4> constructionLeafChildPointers;
    outNode.validMask = 0x0;
    outNode.leafMask = 0x0;
    for (unsigned i = 0; i < constructionInnerNode->children.size(); i++) {
        bool isLeaf;
        if (auto* innerPtr = dynamic_cast<ConstructionInnerNode*>(constructionInnerNode->children[i])) {
            constructionInnerChildPointers.push_back(innerPtr);
            isLeaf = false;
        } else {
            auto* leafPtr = dynamic_cast<ConstructionLeafNode*>(constructionInnerNode->children[i]);
            constructionLeafChildPointers.push_back(leafPtr);
            isLeaf = true;
        }

        outNode.validMask |= 1 << i;
        if (isLeaf)
            outNode.leafMask |= 1 << i;
    }

    // Copy the leafs to their final location
    if (constructionLeafChildPointers.size() > 0) {
        outNode.firstLeafHandle = static_cast<uint32_t>(m_leafs.size());
        for (auto* pConstructionLeafNode : constructionLeafChildPointers) {
            m_leafs.push_back(std::move(leafs[pConstructionLeafNode->leafID]));
        }
    }

    // Recursively generate all the inner node children
    if (constructionInnerChildPointers.size() > 0) {
        auto firstInnerNodeHandle = static_cast<uint32_t>(m_bvhNodes.size());
        for (const auto& _ : constructionInnerChildPointers)
            m_bvhNodes.emplace_back();

        auto handle = firstInnerNodeHandle;
        for (auto* constructionInnerChildPtr : constructionInnerChildPointers) {
            generateFinalBVHRecurse(constructionInnerChildPtr, outHandle, depth + 1, handle++, leafs);
        }

        // m_bvhNodes.push_back() might have reallocated causing BVHNode& outNode to be invalid.
        m_bvhNodes[outHandle].firstChildHandle = firstInnerNodeHandle;
    }
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline std::vector<LeafObj*> PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::collectLeafs() const
{
    // Leafs get moved from their original location and then once more when the leaf allocator is compacted.
    // So we can only collect the final leaf objects after the BVH is created.
    std::vector<LeafObj*> result;
    collectLeafsRecurse(m_innerNodeAllocator.get(m_rootHandle), result);
    return result;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::collectLeafsRecurse(const BVHNode& node, std::vector<LeafObj*>& outLeafObjects) const
{
    unsigned numChildrenReference = 0;
    for (unsigned childIdx = 0; childIdx < 4; childIdx++) {
        if (node.isLeaf(childIdx)) {
            outLeafObjects.push_back(&m_leafAllocator.get(node.getLeafChildHandle(childIdx)));
        } else if (node.isInnerNode(childIdx)) {
            collectLeafsRecurse(m_innerNodeAllocator.get(node.getInnerChildHandle(childIdx)), outLeafObjects);
        }
    }
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void* PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    auto ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode), 8);
    new (ptr) ConstructionInnerNode();
    return ptr;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 4);

    ConstructionInnerNode& node = *reinterpret_cast<ConstructionInnerNode*>(nodePtr);
    node.children.resize(numChildren);
    for (unsigned i = 0; i < numChildren; i++) {
        node.children[i] = reinterpret_cast<ConstructionNode*>(childPtr[i]);
    }
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::innerNodeSetBounds(void* nodePtr, const RTCBounds** embreeBounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 4);

    auto& node = *reinterpret_cast<ConstructionInnerNode*>(nodePtr);
    node.childrenBounds.resize(numChildren);
    for (unsigned i = 0; i < numChildren; i++) {
        Bounds bounds;
        bounds.min.x = embreeBounds[i]->lower_x;
        bounds.min.y = embreeBounds[i]->lower_y;
        bounds.min.z = embreeBounds[i]->lower_z;
        bounds.max.x = embreeBounds[i]->upper_x;
        bounds.max.y = embreeBounds[i]->upper_y;
        bounds.max.z = embreeBounds[i]->upper_z;
        node.childrenBounds[i] = bounds;
    }
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline void* PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims == 1);

    auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);

    void* pMem = rtcThreadLocalAlloc(alloc, sizeof(ConstructionLeafNode), 8);
    auto pLeafNode = new (pMem) ConstructionLeafNode();
    pLeafNode->leafID = prims[0].primID;
    return pLeafNode;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline uint32_t PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::BVHNode::numChildren() const
{
    return _mm_popcnt_u32(validMask);
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline bool PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::BVHNode::isLeaf(unsigned childIdx) const
{
    return (validMask & leafMask) & (1 << childIdx);
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline bool PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::BVHNode::isInnerNode(unsigned childIdx) const
{
    return (validMask & (~leafMask)) & (1 << childIdx);
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline uint32_t PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::BVHNode::getInnerChildHandle(unsigned childIdx) const
{
    uint32_t innerNodeMaskBefore = (validMask & (~leafMask)) & ((1 << childIdx) - 1);
    auto innerNodesBefore = _mm_popcnt_u32(innerNodeMaskBefore);
    return firstChildHandle + innerNodesBefore;
}

template <typename LeafObj, typename HitRayState, typename AnyHitRayState>
inline uint32_t PauseableBVH4<LeafObj, HitRayState, AnyHitRayState>::BVHNode::getLeafChildHandle(unsigned childIdx) const
{
    uint32_t leafNodeMaskBefore = (validMask & leafMask) & ((1 << childIdx) - 1);
    auto leafNodesBefore = _mm_popcnt_u32(leafNodeMaskBefore);
    return firstLeafHandle + leafNodesBefore;
}

}
