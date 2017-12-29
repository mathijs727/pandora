#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh.h"
#include "pandora/traversal/ray.h"
#include <tuple>
#include <vector>

namespace pandora {

template <int N>
class BVHBuilderSAH {
public:
    void build(const std::vector<const TriangleMesh*>& geometry, BVH<N>& bvh);

private:
    struct ObjectSplit {
        int axis;
        float splitLocation;
        Bounds3f leftBounds;
        Bounds3f rightBounds;
    };

    struct PrimitiveBuilder {
        Bounds3f primBounds;
        Vec3f boundsCenter;
        BvhPrimitive primitive;
    };

    struct NodeBuilder {
        typename BVH<N>::InternalNode* nodePtr;
        Bounds3f realBounds;
        Bounds3f centerBounds;
        gsl::span<PrimitiveBuilder> primitives;
    };

    template <int numBins>
    ObjectSplit findObjectSplit(const NodeBuilder& nodeInfo);

    void recurse(const NodeBuilder& nodeInfo);

    void testBvh();

private:
    size_t m_numPrimitives;
    BVH<N>* m_bvh;
};
}