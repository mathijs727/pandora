#pragma once
#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include "pandora/utility/simd/simd8.h"
#include <EASTL/fixed_vector.h>
#include <embree3/rtcore.h>
#include <memory>
#include <tuple>
#include <vector>

namespace pandora {

template <typename LeafObj>
class WiveBVH8 : public BVH<LeafObj> {
public:
    WiveBVH8() = default;
    ~WiveBVH8() = default;

    void addObject(const LeafObj* addObject) override final;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;

protected:
    void testBVH() const;

    static uint32_t createFlagsInner();
    static uint32_t createFlagsLeaf();
    static uint32_t createFlagsEmpty();
    static bool isLeafNode(uint32_t nodePermsAndFlags);
    static bool isInnerNode(uint32_t nodePermsAndFlags);
    static bool isEmptyNode(uint32_t nodePermsAndFlags);

    uint32_t leafNodeChildCount(uint32_t nodeHandle) const;
    static uint32_t signShiftAmount(bool posX, bool posY, bool posZ);
protected:
	struct BVHNode;
	struct BVHLeaf;
private:
	struct SIMDRay;
    void traverseCluster(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_u32& outChildTypes, simd::vec8_f32& outDistances, uint32_t& outNumChildren) const;
    bool intersectLeaf(const BVHLeaf* n, Ray& ray, SurfaceInteraction& si) const;

    struct TestBVHData {
        int numPrimitives;
        std::array<int, 9> numChildrenHistogram;
    };
    void testBVHRecurse(const BVHNode* node, TestBVHData& out) const;

protected:
	enum NodeType : uint32_t {
		EmptyNode = 0b00,
		InnerNode = 0b01,
		LeafNode = 0b10
	};

	struct alignas(64) BVHNode { // 256 bytes (4 cache lines)
		simd::vec8_f32 minX; // 32 bytes
		simd::vec8_f32 maxX; // 32 bytes
		simd::vec8_f32 minY; // 32 bytes
		simd::vec8_f32 maxY; // 32 bytes
		simd::vec8_f32 minZ; // 32 bytes
		simd::vec8_f32 maxZ; // 32 bytes
		simd::vec8_u32 children; // Child indices
		simd::vec8_u32 permOffsetsAndFlags; // Per child: [child flags (1 byte) - permutation offsets (3 bytes)]
	};

	struct alignas(32) BVHLeaf {
		uint32_t leafObjectIDs[4];
		uint32_t primitiveIDs[4];
	};

    const static uint32_t emptyHandle = 0xFFFFFFFF;

    std::vector<const LeafObj*> m_leafObjects;
    std::vector<RTCBuildPrimitive> m_primitives;

    std::unique_ptr<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHNode>> m_innerNodeAllocator;
    std::unique_ptr<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHLeaf>> m_leafNodeAllocator;
    uint32_t m_rootHandle;

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
