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
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    std::vector<const LeafObj*> m_leafObjects;
    std::vector<RTCBuildPrimitive> m_primitives;

    struct ConstructionBVHNode {
        virtual void f(){}; // Need a virtual function to use dynamic_cast
    };

    struct ConstructionInnerNode : public ConstructionBVHNode {
		Bounds childBounds[2];
		const ConstructionBVHNode* children[2];
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
        simd::vec8_i32 permOffsetsAndFlags; // Per child: permutation offsets (3 bytes) + child flags (1 byte)
		// Flags: child type [inner, leaf, empty] (2 bits)  + num primitives if leaf (6 bits)
		simd::vec8_i32 children; // Child indices
    };

    struct alignas(32) LeafNode {
        uint32_t leafObjectIDs[4];
        uint32_t primitiveIDs[4];
    };

    std::unique_ptr<ContiguousAllocatorTS<BVHNode>> m_innerNodeAllocator;
    std::unique_ptr<ContiguousAllocatorTS<LeafNode>> m_leafNodeAllocator;
    const BVHNode* m_root;
private:
	BVHNode* collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds);
	//static BVHNode* convertBinaryTree(const ConstructionInnerNode* root, ContiguousAllocatorTS<BVHNode>& innerNodeAlloc, ContiguousAllocatorTS<LeafNode>& leafNodeAlloc);
};

}

#include "wive_bvh8_impl.h"
