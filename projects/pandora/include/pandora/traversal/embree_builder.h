#pragma once
#include "embree2/rtcore.h"
#include "pandora/geometry/shape.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh.h"
#include "pandora/traversal/ray.h"

namespace pandora {

template <int N>
class BVHBuilderEmbree {
public:
    void build(const std::vector<const TriangleMesh*>& geometry, BVH<N>& bvh);

private:
    static void* createNode(RTCThreadLocalAllocator, size_t numChildren, void* userPtr);
    static void* createLeaf(RTCThreadLocalAllocator, const RTCBuildPrimitive* prims, size_t numPrimitives, void* userPtr);
    static void setNodeChildren(void* nodePtr, void** childPtrs, size_t numChildren, void* userPtr);
    static void setNodeBounds(void* nodePtr, const RTCBounds**, size_t numChildren, void* userPtr);
    //void splitPrimitive(const RTCBuildPrimitive&, unsigned dim, float pos, RTCBounds& lbounds, RTCBounds& rbounds, void* userPtr);
private:
    using NodeRef = typename BVH<N>::NodeRef;

    BVH<N>* m_bvh;
};
}