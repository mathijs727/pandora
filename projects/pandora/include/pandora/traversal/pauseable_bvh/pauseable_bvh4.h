#pragma once
#include "pandora/traversal/pauseable_bvh.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include "pandora/utility/intrinsics.h"
#include "pandora/utility/simd/simd4.h"
#include <embree3/rtcore.h>
#include <limits>
#include <nmmintrin.h> // popcnt
#include <tuple>

namespace pandora {

template <typename LeafObj>
class PauseableBVH4 : PauseableBVH<LeafObj> {
public:
    PauseableBVH4(gsl::span<LeafObj> object, gsl::span<const Bounds> bounds);
    PauseableBVH4(PauseableBVH4&&) = default;
    ~PauseableBVH4() = default;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
    bool intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle handle) const override final;

private:
    struct TestBVHData {
        int numPrimitives = 0;
        int maxDepth = 0;
        std::array<int, 5> numChildrenHistogram = { 0, 0, 0, 0, 0 };
    };
    struct BVHNode;
    void testBVH() const;
    void testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const;

    static void* encodeBVHConstructionLeafHandle(uint32_t handle);
    static void* encodeBVHConstructionInnerNodeHandle(uint32_t handle);
    static std::tuple<bool, uint32_t> decodeBVHConstructionHandle(void* handle);

    void setNodeDepth(BVHNode& node, uint32_t depth);
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    struct alignas(16) BVHNode {
        simd::vec4_f32 minX;
        simd::vec4_f32 minY;
        simd::vec4_f32 minZ;
        simd::vec4_f32 maxX;
        simd::vec4_f32 maxY;
        simd::vec4_f32 maxZ;
        std::array<uint32_t, 4> childrenHandles;
        uint32_t parentHandle;

        uint32_t validMask : 4;
        uint32_t leafMask : 4;
        uint32_t depth : 8;

        uint32_t numChildren() const;
        bool isLeaf(unsigned childIdx) const;
        bool isInnerNode(unsigned childIdx) const;
    };

    ContiguousAllocatorTS<typename PauseableBVH4::BVHNode> m_innerNodeAllocator;
    ContiguousAllocatorTS<LeafObj> m_leafAllocator;
    gsl::span<LeafObj> m_tmpConstructionLeafs;

    uint32_t m_rootHandle;

    static_assert(sizeof(BVHNode) <= 128);
};

template <typename LeafObj>
inline PauseableBVH4<LeafObj>::PauseableBVH4(gsl::span<LeafObj> objects, gsl::span<const Bounds> objectsBounds)
    : m_innerNodeAllocator(objects.size())
    , m_leafAllocator(objects.size())
{
    assert(objects.size() == objectsBounds.size());

    // Copy leafs
    m_tmpConstructionLeafs = objects;

    // Create a representatin of the leafs that Embree will understand
    std::vector<RTCBuildPrimitive> embreeBuildPrimitives;
    embreeBuildPrimitives.reserve(objects.size());
    for (unsigned i = 0; i < static_cast<unsigned>(objects.size()); i++) {
        auto bounds = objectsBounds[i];

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
    auto [isLeaf, nodeHandle] = decodeBVHConstructionHandle(rtcBuildBVH(&arguments));
    assert(!isLeaf);
    m_rootHandle = nodeHandle;
    setNodeDepth(m_innerNodeAllocator.get(nodeHandle), 0);

    // Releases Embree memory (including the temporary BVH)
    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

    m_leafAllocator.compact();
    m_innerNodeAllocator.compact();

    testBVH();
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    return intersect(ray, si, { m_rootHandle, 0xFFFFFFFFFFFFFFFF });
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle insertInfo) const
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
    simdRay.invDirectionX = simd::vec4_f32(1.0f / ray.direction.x);
    simdRay.invDirectionY = simd::vec4_f32(1.0f / ray.direction.y);
    simdRay.invDirectionZ = simd::vec4_f32(1.0f / ray.direction.z);
    simdRay.tnear = simd::vec4_f32(ray.tnear);
    simdRay.tfar = simd::vec4_f32(ray.tfar);
    bool hit = false;

    // Stack
    auto [nodeHandle, rayStack] = insertInfo;
    const BVHNode* node = &m_innerNodeAllocator.get(nodeHandle);
    uint64_t stack = rayStack;
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
				const static simd::vec4_f32 inf4(std::numeric_limits<float>::max());
				const simd::vec4_f32 maskedDistances = simd::blend(inf4, tmin, toVisitMask);
				unsigned childIndex = maskedDistances.horizontalMinIndex();
				assert(childIndex <= 4);

				uint64_t toVisitBitMask = toVisitMask.bitMask();
				toVisitBitMask ^= (1llu << childIndex); // Set the bit of the child we are visiting to 0
				stack = stack ^ (interestBitMask << bitPos); // Set the bits in the stack corresponding to the current node to 0
				stack = stack | (toVisitBitMask << bitPos); // And replace them by the new mask

				if (node->isInnerNode(childIndex)) {
					node = &m_innerNodeAllocator.get(node->childrenHandles[childIndex]);
				} else {
					// Reached leaf
					bool hitBefore = hit;

					auto handle = node->childrenHandles[childIndex];
					const auto& leaf = m_leafAllocator.get(handle);
					if (leaf.intersect(ray, si, insertInfo)) {
						hit = true;
						assert(si.sceneObject);
						simdRay.tfar.broadcast(ray.tfar);
					}

					if (hit)
						assert(si.sceneObject);
				}

				continue;
			}
        }

		if (hit)
			assert(si.sceneObject);

        // No children left to visit; find the first ancestor that has work left

        // Set all bits after bitPos to 1
		uint64_t oldStack = stack;
		stack = stack | (0xFFFFFFFFFFFFFFFF << bitPos);
        if (stack == 0xFFFFFFFFFFFFFFFF)
            break;

		int prevDepth = node->depth;
		node = &m_innerNodeAllocator.get(node->parentHandle);
		assert(node->depth == prevDepth - 1);

        //int index = bitScanReverse64(stack);
		//node = m_innerNodeAllocator.get(node.ancestors[index >> 2]); // (index >> 2) == (index / 4)
    }

	si.wo = -ray.direction;
	assert(!hit || si.sceneObject);

    return hit;
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::testBVH() const
{
    TestBVHData results;
    testBVHRecurse(&m_innerNodeAllocator.get(m_rootHandle), 0, results);

    std::cout << std::endl;
    std::cout << " <<< BVH Build results >>> " << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Primitives reached: " << results.numPrimitives << std::endl;
    std::cout << "Max depth:          " << results.maxDepth << std::endl;
    std::cout << "\nChild count histogram:\n";
    for (size_t i = 0; i < results.numChildrenHistogram.size(); i++)
        std::cout << i << ": " << results.numChildrenHistogram[i] << std::endl;
    std::cout << std::endl;
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const
{
    unsigned numChildrenReference = 0;
    for (unsigned childIdx = 0; childIdx < 4; childIdx++) {
        if (node->isLeaf(childIdx)) {
            out.numPrimitives++;
            numChildrenReference++;
        } else if (node->isInnerNode(childIdx)) {
            testBVHRecurse(&m_innerNodeAllocator.get(node->childrenHandles[childIdx]), depth + 1, out);
            numChildrenReference++;
        }
    }

    unsigned numChildren = node->numChildren();
    assert(numChildren == numChildrenReference);

    assert(depth == node->depth);
    out.maxDepth = std::max(out.maxDepth, depth);
    out.numChildrenHistogram[numChildren]++;
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::encodeBVHConstructionLeafHandle(uint32_t handle)
{
    assert((handle & 0x7FFFFFFF) == handle);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(handle | 0x80000000)); // Highest bit set to 1, indicating a leaf
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::encodeBVHConstructionInnerNodeHandle(uint32_t handle)
{
    assert((handle & 0x7FFFFFFF) == handle);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(handle & 0x7FFFFFFF)); // Highest bit set to 0, indicating an inner node
}

template <typename LeafObj>
inline std::tuple<bool, uint32_t> PauseableBVH4<LeafObj>::decodeBVHConstructionHandle(void* encodedHandlePtr)
{
    uint32_t encodedHandle = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(encodedHandlePtr));
    bool isLeaf = encodedHandle & 0x80000000;
    uint32_t handle = encodedHandle & 0x7FFFFFFF;
    return { isLeaf, handle };
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::setNodeDepth(BVHNode& node, uint32_t depth)
{
    node.depth = depth;

    for (unsigned childIdx = 0; childIdx < 4; childIdx++) {
        if (node.isInnerNode(childIdx)) {
            setNodeDepth(m_innerNodeAllocator.get(node.childrenHandles[childIdx]), depth + 1);
        }
    }
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
    auto [nodeHandle, nodePtr] = self->m_innerNodeAllocator.allocate();
    (void)nodePtr;

    return encodeBVHConstructionInnerNodeHandle(nodeHandle);
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 4);

    auto [isLeaf, nodeHandle] = decodeBVHConstructionHandle(nodePtr);
    assert(!isLeaf);
    auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
    BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

    uint32_t validMask = 0x0;
    uint32_t leafMask = 0x0;
    for (unsigned i = 0; i < numChildren; i++) {
        auto [isLeaf, childNodeHandle] = decodeBVHConstructionHandle(childPtr[i]);

        validMask |= 1 << i;
        if (isLeaf) {
            leafMask |= 1 << i;
        } else {
			BVHNode& childNode = self->m_innerNodeAllocator.get(childNodeHandle);
            childNode.parentHandle = nodeHandle;
        }

        node.childrenHandles[i] = childNodeHandle;
    }
    node.validMask = validMask;
    node.leafMask = leafMask;
}

template <typename LeafObj>
inline void PauseableBVH4<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 4);

    auto [isLeaf, nodeHandle] = decodeBVHConstructionHandle(nodePtr);
    assert(!isLeaf);
    auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
    BVHNode& node = self->m_innerNodeAllocator.get(nodeHandle);

    std::array<float, 4> minX, minY, minZ, maxX, maxY, maxZ;
    for (unsigned i = 0; i < numChildren; i++) {
        const RTCBounds* childBounds = bounds[i];
        minX[i] = childBounds->lower_x;
        minY[i] = childBounds->lower_y;
        minZ[i] = childBounds->lower_z;
        maxX[i] = childBounds->upper_x;
        maxY[i] = childBounds->upper_y;
        maxZ[i] = childBounds->upper_z;
    }
    for (unsigned i = numChildren; i < 4; i++) {
        minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
    }

    node.minX.load(minX);
    node.minY.load(minY);
    node.minZ.load(minZ);
    node.maxX.load(maxX);
    node.maxY.load(maxY);
    node.maxZ.load(maxZ);
}

template <typename LeafObj>
inline void* PauseableBVH4<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims == 1);

    // Allocate node
    auto* self = reinterpret_cast<PauseableBVH4*>(userPtr);
    auto [nodeHandle, nodePtr] = self->m_leafAllocator.allocate();
    *nodePtr = std::move(self->m_tmpConstructionLeafs[prims[0].primID]);
    return encodeBVHConstructionLeafHandle(nodeHandle);
}

template <typename LeafObj>
inline uint32_t PauseableBVH4<LeafObj>::BVHNode::numChildren() const
{
    return _mm_popcnt_u32(validMask);
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::BVHNode::isLeaf(unsigned childIdx) const
{
    return (validMask & leafMask) & (1 << childIdx);
}

template <typename LeafObj>
inline bool PauseableBVH4<LeafObj>::BVHNode::isInnerNode(unsigned childIdx) const
{
    return (validMask & (~leafMask)) & (1 << childIdx);
}

}
