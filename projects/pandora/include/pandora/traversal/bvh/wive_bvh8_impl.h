#include "pandora/flatbuffers/wive_bvh8_generated.h"
#include <EASTL/fixed_map.h>
#include <EASTL/fixed_set.h>
#include <EASTL/fixed_vector.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <mio/mmap.hpp>
#include "wive_bvh8.h"

namespace pandora {

template <typename LeafObj>
inline WiVeBVH8<LeafObj>::WiVeBVH8(uint32_t numPrims)
    : m_innerNodeAllocator(std::max(16u, numPrims / 2), 16)
    , m_leafIndexAllocator(numPrims + numPrims / 2)
{
}

template <typename LeafObj>
inline WiVeBVH8<LeafObj>::WiVeBVH8(const serialization::WiVeBVH8* serialized, std::span<const LeafObj> leafs)
    : m_innerNodeAllocator(serialized->innerNodeAllocator())
    , m_leafIndexAllocator(serialized->leafIndexAllocator())
{
    m_compressedRootHandle = serialized->compressedRootHandle();

    size_t numNodesGiven = leafs.size();
    size_t numNodesSerialized = serialized->numLeafObjects();
    assert(numNodesGiven > 0);
    assert(m_leafObjects.empty());
    ALWAYS_ASSERT(numNodesGiven == numNodesSerialized, "Number of leaf objects does not match that of the serialized BVH");

    m_leafObjects.reserve(leafs.size());
    std::copy(std::begin(leafs), std::end(leafs), std::back_inserter(m_leafObjects));
}

template <typename LeafObj>
inline flatbuffers::Offset<serialization::WiVeBVH8> WiVeBVH8<LeafObj>::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    auto serializedInnerNodeAllocator = m_innerNodeAllocator.serialize(builder);
    auto serializedLeafIndexAllocator = m_leafIndexAllocator.serialize(builder);
    assert(!this->m_leafObjects.empty());
    return serialization::CreateWiVeBVH8(
        builder,
        serializedInnerNodeAllocator,
        serializedLeafIndexAllocator,
        m_compressedRootHandle,
        this->m_leafObjects.size());
}

template <typename LeafObj>
inline size_t WiVeBVH8<LeafObj>::sizeBytes() const
{
    return sizeof(decltype(*this)) + m_innerNodeAllocator.sizeBytes() + m_leafIndexAllocator.sizeBytes();
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    bool hit = false;

    SIMDRay simdRay;
    simdRay.originX = simd::vec8_f32(ray.origin.x);
    simdRay.originY = simd::vec8_f32(ray.origin.y);
    simdRay.originZ = simd::vec8_f32(ray.origin.z);
    simdRay.invDirectionX = simd::vec8_f32(1.0f / ray.direction.x);
    simdRay.invDirectionY = simd::vec8_f32(1.0f / ray.direction.y);
    simdRay.invDirectionZ = simd::vec8_f32(1.0f / ray.direction.z);
    simdRay.tnear = simd::vec8_f32(ray.tnear);
    simdRay.tfar = simd::vec8_f32(ray.tfar);
    simdRay.raySignShiftAmount = simd::vec8_u32(signShiftAmount(ray.direction.x > 0, ray.direction.y > 0, ray.direction.z > 0));

    // Stack
    alignas(32) std::array<uint32_t, 96> stackCompressedNodeHandles;
    alignas(32) std::array<float, 96> stackDistances;
    std::fill(std::begin(stackDistances), std::end(stackDistances), std::numeric_limits<float>::max());
    size_t stackPtr = 0;

    // Push root node onto the stack
    stackCompressedNodeHandles[stackPtr] = m_compressedRootHandle;
    stackDistances[stackPtr] = 0.0f;
    stackPtr++;

    while (stackPtr > 0) {
        stackPtr--;
        uint32_t compressedNodeHandle = stackCompressedNodeHandles[stackPtr];
        float distance = stackDistances[stackPtr];

        uint32_t handle = decompressNodeHandle(compressedNodeHandle);
        const auto* node = &m_innerNodeAllocator.get(handle);
        if (isInnerNode(compressedNodeHandle)) {
            // Inner node
            simd::vec8_u32 childrenSIMD;
            simd::vec8_f32 distancesSIMD;
            uint32_t numChildren = intersectInnerNode(node, simdRay, childrenSIMD, distancesSIMD);

            if (numChildren > 0) {
                childrenSIMD.store(std::span(stackCompressedNodeHandles.data() + stackPtr, 8));
                distancesSIMD.store(std::span(stackDistances.data() + stackPtr, 8));

                stackPtr += numChildren;
            }
        } else {
#ifndef NDEBUG
            if (isEmptyNode(compressedNodeHandle))
                THROW_ERROR("Empty node in traversal");
            assert(isLeafNode(compressedNodeHandle));
#endif
            // Leaf node
            if (intersectLeaf(&m_leafIndexAllocator.get(handle), leafNodePrimitiveCount(compressedNodeHandle), ray, si)) {
                hit = true;
                simdRay.tfar.broadcast(ray.tfar);

                // Compress stack
                size_t outStackPtr = 0;
                for (size_t i = 0; i < stackPtr; i += 8) {
                    simd::vec8_u32 nodesSIMD;
                    simd::vec8_f32 distancesSIMD;
                    distancesSIMD.loadAligned(std::span(stackDistances.data() + i, 8));
                    nodesSIMD.loadAligned(std::span(stackCompressedNodeHandles.data() + i, 8));

                    simd::mask8 distMask = distancesSIMD < simdRay.tfar;
                    simd::vec8_u32 compressPermuteIndices(distMask.computeCompressPermutation()); // Compute permute indices that represent the compression (so we only have to calculate them once)
                    distancesSIMD = distancesSIMD.permute(compressPermuteIndices);
                    nodesSIMD = nodesSIMD.permute(compressPermuteIndices);

                    distancesSIMD.store(std::span(stackDistances.data() + outStackPtr, 8));
                    nodesSIMD.store(std::span(stackCompressedNodeHandles.data() + outStackPtr, 8));

                    size_t numItems = std::min((size_t)8, stackPtr - i);
                    unsigned validMask = (1 << numItems) - 1;
                    outStackPtr += distMask.count(validMask);
                }
                stackPtr = outStackPtr;
            }
        }
    }

    return hit;
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::intersectAny(Ray& ray) const
{
    SIMDRay simdRay;
    simdRay.originX = simd::vec8_f32(ray.origin.x);
    simdRay.originY = simd::vec8_f32(ray.origin.y);
    simdRay.originZ = simd::vec8_f32(ray.origin.z);
    simdRay.invDirectionX = simd::vec8_f32(1.0f / ray.direction.x);
    simdRay.invDirectionY = simd::vec8_f32(1.0f / ray.direction.y);
    simdRay.invDirectionZ = simd::vec8_f32(1.0f / ray.direction.z);
    simdRay.tnear = simd::vec8_f32(ray.tnear);
    simdRay.tfar = simd::vec8_f32(ray.tfar);
    simdRay.raySignShiftAmount = simd::vec8_u32(signShiftAmount(ray.direction.x > 0, ray.direction.y > 0, ray.direction.z > 0));

    // Stack
    alignas(32) std::array<uint32_t, 96> stackCompressedNodeHandles;
    alignas(32) std::array<float, 96> stackDistances;
    std::fill(std::begin(stackDistances), std::end(stackDistances), std::numeric_limits<float>::max());
    size_t stackPtr = 0;

    // Push root node onto the stack
    stackCompressedNodeHandles[stackPtr] = m_compressedRootHandle;
    stackDistances[stackPtr] = 0.0f;
    stackPtr++;

    while (stackPtr > 0) {
        stackPtr--;
        uint32_t compressedNodeHandle = stackCompressedNodeHandles[stackPtr];
        float distance = stackDistances[stackPtr];

        uint32_t handle = decompressNodeHandle(compressedNodeHandle);
        const auto* node = &m_innerNodeAllocator.get(handle);
        if (isInnerNode(compressedNodeHandle)) {
            // Inner node
            simd::vec8_u32 childrenSIMD;
            simd::vec8_f32 distancesSIMD;
            uint32_t numChildren = intersectInnerNode(node, simdRay, childrenSIMD, distancesSIMD);

            if (numChildren > 0) {
                childrenSIMD.store(std::span(stackCompressedNodeHandles.data() + stackPtr, 8));
                distancesSIMD.store(std::span(stackDistances.data() + stackPtr, 8));

                stackPtr += numChildren;
            }
        } else {
#ifndef NDEBUG
            if (isEmptyNode(compressedNodeHandle))
                THROW_ERROR("Empty node in traversal");
            assert(isLeafNode(compressedNodeHandle));
#endif
            // Leaf node
            if (intersectAnyLeaf(&m_leafIndexAllocator.get(handle), leafNodePrimitiveCount(compressedNodeHandle), ray)) {
                ray.tfar = -std::numeric_limits<float>::infinity();
                return true;
            }
        }
    }

    return false;
}

template <typename LeafObj>
inline std::span<const LeafObj> WiVeBVH8<LeafObj>::leafs() const
{
    return m_leafObjects;
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::intersectInnerNode(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_f32& outDistances) const
{
    simd::vec8_f32 tx1 = (n->minX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 tx2 = (n->maxX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 ty1 = (n->minY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 ty2 = (n->maxY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 tz1 = (n->minZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 tz2 = (n->maxZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 txMin = simd::min(tx1, tx2);
    simd::vec8_f32 tyMin = simd::min(ty1, ty2);
    simd::vec8_f32 tzMin = simd::min(tz1, tz2);
    simd::vec8_f32 txMax = simd::max(tx1, tx2);
    simd::vec8_f32 tyMax = simd::max(ty1, ty2);
    simd::vec8_f32 tzMax = simd::max(tz1, tz2);
    simd::vec8_f32 tmin = simd::max(ray.tnear, simd::max(txMin, simd::max(tyMin, tzMin)));
    simd::vec8_f32 tmax = simd::min(ray.tfar, simd::min(txMax, simd::min(tyMax, tzMax)));

    const simd::vec8_u32 indexMask(0b111);
    const simd::vec8_u32 simd24(24);
    simd::vec8_u32 index = (n->permutationOffsets >> ray.raySignShiftAmount) & indexMask;

    tmin = tmin.permute(index);
    tmax = tmax.permute(index);
    simd::mask8 mask = tmin <= tmax;
    simd::vec8_u32 compressPermuteIndices(mask.computeCompressPermutation());
    outChildren = n->children.permute(index).permute(compressPermuteIndices);
    outDistances = tmin.permute(compressPermuteIndices);
    return mask.count();
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::intersectAnyInnerNode(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_f32& outDistances) const
{
    simd::vec8_f32 tx1 = (n->minX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 tx2 = (n->maxX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 ty1 = (n->minY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 ty2 = (n->maxY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 tz1 = (n->minZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 tz2 = (n->maxZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 txMin = simd::min(tx1, tx2);
    simd::vec8_f32 tyMin = simd::min(ty1, ty2);
    simd::vec8_f32 tzMin = simd::min(tz1, tz2);
    simd::vec8_f32 txMax = simd::max(tx1, tx2);
    simd::vec8_f32 tyMax = simd::max(ty1, ty2);
    simd::vec8_f32 tzMax = simd::max(tz1, tz2);
    simd::vec8_f32 tmin = simd::max(ray.tnear, simd::max(txMin, simd::max(tyMin, tzMin)));
    simd::vec8_f32 tmax = simd::min(ray.tfar, simd::min(txMax, simd::min(tyMax, tzMax)));

    // Disable sorting since any hit will do
    //const simd::vec8_u32 indexMask(0b111);
    //const simd::vec8_u32 simd24(24);
    //simd::vec8_u32 index = (n->permutationOffsets >> ray.raySignShiftAmount) & indexMask;
    //tmin = tmin.permute(index);
    //tmax = tmax.permute(index);

    simd::mask8 mask = tmin <= tmax;
    simd::vec8_u32 compressPermuteIndices(mask.computeCompressPermutation());
    outChildren = n->children.permute(compressPermuteIndices);
    outDistances = tmin.permute(compressPermuteIndices);
    return mask.count();
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::intersectLeaf(const uint32_t* leafObjectIndices, uint32_t objectCount, Ray& ray, SurfaceInteraction& si) const
{
    bool hit = false;
    for (uint32_t i = 0; i < objectCount; i++) {
        hit |= m_leafObjects[leafObjectIndices[i]].intersect(ray, si);
    }
    return hit;
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::intersectAnyLeaf(const uint32_t* leafObjectIndices, uint32_t objectCount, Ray& ray) const
{
    for (uint32_t i = 0; i < objectCount; i++) {
        if (m_leafObjects[leafObjectIndices[i]].intersectAny(ray))
            return true;
    }
    return false;
}

template <typename LeafObj>
inline void WiVeBVH8<LeafObj>::testBVH() const
{
    TestBVHData results;
    if (isInnerNode(m_compressedRootHandle))
        testBVHRecurse(&m_innerNodeAllocator->get(decompressNodeHandle(m_compressedRootHandle)), 1, results);
    else
        std::cout << "ROOT IS LEAF NODE" << std::endl;

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
inline void WiVeBVH8<LeafObj>::testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const
{
    std::array<uint32_t, 8> children;
    node->children.store(children);

    int numChildren = 0;
    for (int i = 0; i < 8; i++) {
        if (isLeafNode(children[i])) {
            out.numPrimitives += leafNodePrimitiveCount(children[i]);
        } else if (isInnerNode(children[i])) {
            testBVHRecurse(&m_innerNodeAllocator->get(decompressNodeHandle(children[i])), depth + 1, out);
        }

        if (!isEmptyNode(children[i]))
            numChildren++;
    }
    out.maxDepth = std::max(out.maxDepth, depth);
    out.numChildrenHistogram[numChildren]++;
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::compressHandleInner(uint32_t handle)
{
    assert(handle < (1 << 29) - 1);
    return (handle & ((1 << 29) - 1)) | (0b010 << 29);
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::compressHandleLeaf(uint32_t handle, uint32_t primCount)
{
    assert(primCount > 0 && primCount <= 4);
    assert(handle < (1 << 29) - 1);
    return (handle & ((1 << 29) - 1)) | ((0b100 | (primCount - 1)) << 29);
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::compressHandleEmpty()
{
    return 0u; // 0 | (0b000 << 29)
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::isLeafNode(uint32_t compressedHandle)
{
    return ((compressedHandle >> 29) & 0b100) == 0b100; // Other 2 bits are used to store the primitive count
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::isInnerNode(uint32_t compressedHandle)
{
    return ((compressedHandle >> 29) & 0b111) == 0b010;
}

template <typename LeafObj>
inline bool WiVeBVH8<LeafObj>::isEmptyNode(uint32_t compressedHandle)
{
    return ((compressedHandle >> 29) & 0b111) == 0b000;
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::decompressNodeHandle(uint32_t compressedHandle)
{
    return compressedHandle & ((1u << 29) - 1);
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::leafNodePrimitiveCount(uint32_t compressedHandle)
{
    return ((compressedHandle >> 29) & 0b011) + 1;
}

template <typename LeafObj>
inline uint32_t WiVeBVH8<LeafObj>::signShiftAmount(bool positiveX, bool positiveY, bool positiveZ)
{
    return ((positiveX ? 0b001 : 0u) | (positiveY ? 0b010 : 0u) | (positiveZ ? 0b100 : 0u)) * 3;
}

}
