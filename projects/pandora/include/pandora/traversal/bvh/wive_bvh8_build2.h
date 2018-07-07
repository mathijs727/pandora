#pragma once
#include "pandora/utility/error_handling.h"
#include "wive_bvh8.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <variant>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8Build2 : public WiVeBVH8<LeafObj> {
protected:
    void commit() override final;

private:
    struct ConstructionBVHNode {
        virtual void f(){}; // Need a virtual function to use dynamic_cast
    };

    struct ConstructionInnerNode : public ConstructionBVHNode {
        Bounds childBounds[2];
        ConstructionBVHNode* children[2];
        int splitAxis;
    };

    struct ConstructionLeafNode : public ConstructionBVHNode {
        eastl::fixed_vector<std::pair<uint32_t, uint32_t>, 4> leafs;
    };

private:
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

    void orderChildrenConstructionBVH(ConstructionBVHNode* node);
    std::pair<uint32_t, const typename WiVeBVH8<LeafObj>::BVHNode*> collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds);
};

#include "wive_bvh8_build2_impl.h"

}
