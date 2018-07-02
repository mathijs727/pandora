#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include "pandora/utility/simd/simd8.h"
#include <embree3/rtcore.h>
#include <memory>
#include <tuple>
#include <vector>

namespace pandora {

template <typename LeafObj>
class WiveBVH8 : public BVH<LeafObj> {
public:
    WiveBVH8();
    ~WiveBVH8();

    void addObject(const LeafObj* addObject) override final;
    void commit() override final;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
private:
	struct SIMDRay
	{
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
	struct BVHNode;
	struct BVHLeaf;
	void traverseCluster(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_u32& outChildTypes, simd::vec8_f32& outDistances, uint32_t& outNumChildren) const;
	bool intersectLeaf(const BVHLeaf* n, Ray& ray, SurfaceInteraction& si) const;

    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

	struct TestBVHData
	{
		int numPrimitives;
		std::array<int, 9> numChildrenHistogram;
	};
	void testBVH() const;
	struct BVHNode;
	void testBVHRecurse(const BVHNode* node, TestBVHData& out) const;
private:
    std::vector<const LeafObj*> m_leafObjects;
    std::vector<RTCBuildPrimitive> m_primitives;

    struct ConstructionBVHNode {
        virtual void f(){}; // Need a virtual function to use dynamic_cast
    };

    struct ConstructionInnerNode : public ConstructionBVHNode {
        Bounds childBounds[2];
        ConstructionBVHNode* children[2];
        int splitAxis = 0;
    };

    struct ConstructionLeafNode : public ConstructionBVHNode {
        eastl::fixed_vector<std::pair<uint32_t, uint32_t>, 4> leafs;
    };

    struct alignas(64) BVHNode { // 256 bytes (4 cache lines)
        simd::vec8_f32 minX; // 32 bytes
        simd::vec8_f32 maxX; // 32 bytes
        simd::vec8_f32 minY; // 32 bytes
        simd::vec8_f32 maxY; // 32 bytes
        simd::vec8_f32 minZ; // 32 bytes
        simd::vec8_f32 maxZ; // 32 bytes
        simd::vec8_u32 permOffsetsAndFlags; // Per child: [child flags (1 byte) - permutation offsets (3 bytes)]
        simd::vec8_u32 children; // Child indices
    };

	const static uint32_t emptyHandle = 0xFFFFFFFF;
    struct alignas(32) BVHLeaf {
        uint32_t leafObjectIDs[4];
        uint32_t primitiveIDs[4];
    };

    std::unique_ptr<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHNode>> m_innerNodeAllocator;
    std::unique_ptr<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHLeaf>> m_leafNodeAllocator;
    uint32_t m_rootHandle;

private:
	void orderChildrenConstructionBVH( ConstructionBVHNode* node);
    std::pair<uint32_t, const WiveBVH8<LeafObj>::BVHNode*> collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds);
    //static BVHNode* convertBinaryTree(const ConstructionInnerNode* root, ContiguousAllocatorTS<BVHNode>& innerNodeAlloc, ContiguousAllocatorTS<LeafNode>& leafNodeAlloc);

	enum NodeType : uint32_t {
		EmptyNode = 0b00,
		InnerNode = 0b01,
		LeafNode = 0b10
	};

	static uint32_t createFlagsInner();
	static uint32_t createFlagsLeaf();
	static uint32_t createFlagsEmpty();
	static bool isLeafNode(uint32_t nodePermsAndFlags);
	static bool isInnerNode(uint32_t nodePermsAndFlags);
	static bool isEmptyNode(uint32_t nodePermsAndFlags);

	uint32_t leafNodeChildCount(uint32_t nodeHandle) const;
	static uint32_t signShiftAmount(bool posX, bool posY, bool posZ);
};

}

#include "wive_bvh8_impl.h"


