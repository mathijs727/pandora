#include "pandora/traversal/embree_builder.h"
#include "pandora/traversal/test_bvh.h"

namespace pandora {

template <int N>
void BVHBuilderEmbree<N>::build(const std::vector<const TriangleMesh*>& geometry, BVH<N>& bvh)
{
    m_bvh = &bvh;

    // TODO: parallelize and write this using STL algorithms?
    bvh.m_numPrimitives = 0;
    std::vector<RTCBuildPrimitive> primitives;
    for (uint32_t geomID = 0; geomID < (uint32_t)geometry.size(); geomID++) {
        auto primBounds = geometry[geomID]->getPrimitivesBounds();
        primitives.reserve(primitives.size() + primBounds.size());
        bvh.m_numPrimitives += primitives.size();

        for (uint32_t primID = 0; primID < (uint32_t)primBounds.size(); primID++) {
            auto bounds = primBounds[primID];
            RTCBuildPrimitive embreePrimitive;
            embreePrimitive.lower_x = bounds.bounds_min.x;
            embreePrimitive.lower_y = bounds.bounds_min.y;
            embreePrimitive.lower_z = bounds.bounds_min.z;
            embreePrimitive.upper_x = bounds.bounds_max.x;
            embreePrimitive.upper_y = bounds.bounds_max.y;
            embreePrimitive.upper_z = bounds.bounds_max.z;
            embreePrimitive.geomID = (int)geomID;
            embreePrimitive.primID = (int)primID;
        }
    }

    RTCDevice device = rtcNewDevice();
    RTCBVH embreeBvh = rtcNewBVH(device);
    RTCBuildSettings buildSettings = rtcDefaultBuildSettings();
    buildSettings.maxBranchingFactor = N;
    buildSettings.quality = RTC_BUILD_QUALITY_NORMAL;
    void* rootNodeRefPtr = rtcBuildBVH(
        embreeBvh,
        buildSettings,
        primitives.data(),
        primitives.size(),
        &createNode,
        &setNodeChildren,
        &setNodeBounds,
        &createLeaf,
        nullptr, // TODO: primitive split
        nullptr, // Build progress
        this);
    bvh.m_rootNode = *reinterpret_cast<NodeRef*>(rootNodeRefPtr); // Before rtcDeleteBVH
    rtcDeleteBVH(embreeBvh);

    testBvh(bvh);
}

template <int N>
void* BVHBuilderEmbree<N>::createNode(RTCThreadLocalAllocator tmpAllocator, size_t numChildren, void* userPtr)
{
    auto* thisPtr = reinterpret_cast<BVHBuilderEmbree<N>*>(userPtr);

    auto& allocator = thisPtr->m_bvh->m_nodeAllocator;
    auto* innerNode = allocator.template allocate<typename BVH<N>::InternalNode>(1, 32);

    // We want to return a NodeRef but that is 8 bytes so casting it to a void* won't work on 32 bit platforms.
    // Instead we use the fast Embree allocator to allocate temporary storage for the NodeReferences.
    // This memory will automatically be freed when rtcDeleteBVH() is called.
    auto* nodeRefPtr = reinterpret_cast<NodeRef*>(rtcThreadLocalAlloc(tmpAllocator, sizeof(NodeRef), 0));
    *nodeRefPtr = NodeRef(innerNode);
    return nodeRefPtr;
}

template <int N>
void* BVHBuilderEmbree<N>::createLeaf(RTCThreadLocalAllocator tmpAllocator, const RTCBuildPrimitive* prims, size_t numPrimitives, void* userPtr)
{
    auto* thisPtr = reinterpret_cast<BVHBuilderEmbree<N>*>(userPtr);

    // Allocate space for the primitives
    auto& allocator = thisPtr->m_bvh->m_primitiveAllocator;
    auto* primitives = allocator.template allocate<BvhPrimitive>(numPrimitives, 32);

    for (size_t i = 0; i < numPrimitives; i++)
        primitives[i] = BvhPrimitive((uint32_t)prims[i].geomID, (uint32_t)prims[i].primID);

    // We want to return a NodeRef but that is 8 bytes so casting it to a void* won't work on 32 bit platforms.
    // Instead we use the fast Embree allocator to allocate temporary storage for the NodeReferences.
    // This memory will automatically be freed when rtcDeleteBVH() is called.
    auto* nodeRefPtr = reinterpret_cast<NodeRef*>(rtcThreadLocalAlloc(tmpAllocator, sizeof(NodeRef), 0));
    *nodeRefPtr = NodeRef(primitives, numPrimitives);
    return nodeRefPtr;
}

template <int N>
void BVHBuilderEmbree<N>::setNodeChildren(void* voidNodePtr, void** childPtrs, size_t numChildren, void* userPtr)
{
    NodeRef* nodeRef = reinterpret_cast<NodeRef*>(voidNodePtr);
    NodeRef** childRefs = reinterpret_cast<NodeRef**>(childPtrs);

    // Leaf nodes cant have children
    assert(nodeRef->isInternalNode());

    auto* node = nodeRef->getInternalNode();
    node->numChildren = numChildren;
    for (size_t childIdx = 0; childIdx < numChildren; childIdx++) {
        node->children[childIdx] = *childRefs[childIdx];
    }
}

template <int N>
void BVHBuilderEmbree<N>::setNodeBounds(void* voidNodePtr, const RTCBounds** bounds, size_t numChildren, void* userPtr)
{
    NodeRef* nodeRef = reinterpret_cast<NodeRef*>(voidNodePtr);

    // Leaf nodes cant have children
    assert(nodeRef->isInternalNode());

    auto* node = nodeRef->getInternalNode();
    assert((size_t)node->numChildren == numChildren);
    for (size_t childIdx = 0; childIdx < numChildren; childIdx++) {
        RTCBounds embreeBounds = *bounds[childIdx];
        Vec3f lower = Vec3f(embreeBounds.lower_x, embreeBounds.lower_y, embreeBounds.lower_z);
        Vec3f upper = Vec3f(embreeBounds.upper_x, embreeBounds.upper_y, embreeBounds.upper_z);
        node->childBounds[childIdx] = Bounds3f(lower, upper);
    }
}

template class BVHBuilderEmbree<2>;
}