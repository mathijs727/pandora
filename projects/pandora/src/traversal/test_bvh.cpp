#include "pandora/traversal/test_bvh.h"
#include <unordered_map>

namespace pandora {
void testBvh(BVH<2>& bvh)
{
    // Test that we can reach all the primitives
    // TODO: gsl_span gives a bunch of compile errors when unordered_map is included so use std::map for now
    std::unordered_map<uint64_t, bool> reachablePrims;

    std::vector<BVH<2>::NodeRef> traversalStack = { bvh.m_rootNode };
    while (!traversalStack.empty()) {
        auto nodeRef = traversalStack.back();
        traversalStack.pop_back();

        if (nodeRef.isInternalNode()) {
            auto* nodePtr = nodeRef.getInternalNode();
            if (nodePtr->numChildren != 2) {
                std::cout << "Internal node with " << nodePtr->numChildren << "children" << std::endl;
                exit(1);
            }
            traversalStack.push_back(nodePtr->children[0]);
            traversalStack.push_back(nodePtr->children[1]);
        } else if (nodeRef.isLeaf()) {
            auto* primitives = nodeRef.getPrimitives();

            // Node refers to one or more primitives
            for (uint64_t i = 0; i < nodeRef.numPrimitives(); i++) {
                auto [geomID, primID] = primitives[i];
                uint64_t hashKey = (((uint64_t)geomID) << 32) + (uint64_t)primID;
                reachablePrims[hashKey] = true;
            }
        } // TODO: transform nodes
    }

    size_t reachable = std::accumulate(std::begin(reachablePrims), std::end(reachablePrims), 0,
        [](size_t counter, const decltype(reachablePrims)::value_type& p) {
            return counter + p.second;
        });
    std::cout << reachable << " out of " << bvh.m_numPrimitives << " primitives reachable" << std::endl;

    if (reachable != bvh.m_numPrimitives)
        exit(1);
}
}