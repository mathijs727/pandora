#pragma once
#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include "pandora/utility/simd/simd8.h"
#include <EASTL/fixed_vector.h>
#include <embree3/rtcore.h>
#include <iostream>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8 : public BVH<LeafObj> {
public:
	WiVeBVH8() = default;
	WiVeBVH8(WiVeBVH8&&) = default;
    ~WiVeBVH8() = default;

    void build(gsl::span<const LeafObj*> objects) override final;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;

	void loadFromFile(std::string_view filename, gsl::span<const LeafObj*> objects);
	void saveToFile(std::string_view filename);
protected:
	virtual void commit() = 0;

    void testBVH() const;

    // 32 bits for node + flags
    // 29 bit node handle: ~500.000.000 inner nodes / leaf nodes (counting seperately) max -> should be enough
    // 3 bit flags: [000] = empty node, [010] = inner node, [1xx] is leaf node where xx = number of primitives - 1
    // [flags+prim count (3 bits) - handle (29 bits)]
    static uint32_t compressHandleInner(uint32_t handle);
    static uint32_t compressHandleLeaf(uint32_t handle, uint32_t primCount);
    static uint32_t compressHandleEmpty();
    static bool isLeafNode(uint32_t compressedHandle);
    static bool isInnerNode(uint32_t compressedHandle);
    static bool isEmptyNode(uint32_t compressedHandle);
    static uint32_t decompressNodeHandle(uint32_t compressedHandle);
    static uint32_t leafNodePrimitiveCount(uint32_t compressedHandle);

    static uint32_t signShiftAmount(bool posX, bool posY, bool posZ);

protected:
    struct BVHNode;
    struct BVHLeaf;

private:

    struct SIMDRay;
    uint32_t traverseCluster(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_f32& outDistances) const;
    bool intersectLeaf(const BVHLeaf* n, uint32_t primitiveCount, Ray& ray, SurfaceInteraction& si) const;

    struct TestBVHData {
        int numPrimitives = 0;
        int maxDepth = 0;
        std::array<int, 9> numChildrenHistogram = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    };
    void testBVHRecurse(const BVHNode* node, int depth, TestBVHData& out) const;

protected:
    struct alignas(64) BVHNode { // 256 bytes (4 cache lines)
        simd::vec8_f32 minX; // 32 bytes
        simd::vec8_f32 maxX; // 32 bytes
        simd::vec8_f32 minY; // 32 bytes
        simd::vec8_f32 maxY; // 32 bytes
        simd::vec8_f32 minZ; // 32 bytes
        simd::vec8_f32 maxZ; // 32 bytes
        simd::vec8_u32 children; // Child indices
        //simd::vec8_u32 permOffsetsAndFlags; // Per child: [child flags (1 byte) - permutation offsets (3 bytes)]
        simd::vec8_u32 permutationOffsets; // 3 bytes. Can use the other byte for flags but storing it on the stack during traversal is expensive.
    };

    struct alignas(32) BVHLeaf {
        uint32_t leafObjectIDs[4];
        uint32_t primitiveIDs[4];
    };

    const static uint32_t emptyHandle = 0xFFFFFFFF;

    std::vector<const LeafObj*> m_leafObjects;
    std::vector<RTCBuildPrimitive> m_primitives;

    std::unique_ptr<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHNode>> m_innerNodeAllocator;
    std::unique_ptr<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHLeaf>> m_leafNodeAllocator;
    uint32_t m_compressedRootHandle;

private:
    struct SIMDRay {
        simd::vec8_f32 originX;
        simd::vec8_f32 originY;
        simd::vec8_f32 originZ;

        simd::vec8_f32 invDirectionX;
        simd::vec8_f32 invDirectionY;
        simd::vec8_f32 invDirectionZ;

        simd::vec8_f32 tnear;
        simd::vec8_f32 tfar;
        simd::vec8_u32 raySignShiftAmount;
    };
};

}

#include "wive_bvh8_impl.h"
