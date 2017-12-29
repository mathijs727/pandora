#include "pandora/traversal/sbvh.h"
#include <algorithm>
#include <iostream>
#include <unordered_map>

namespace pandora {

template <int N>
void BVHBuilderSAH<N>::build(const std::vector<const TriangleMesh*>& geometry, BVH<N>& bvh)
{
    m_bvh = &bvh;

    // Store pointers to the geometry in the BVH
    bvh.m_shapes.resize(geometry.size());
    std::copy(geometry.begin(), geometry.end(), bvh.m_shapes.begin());

    // TODO: instead of propegating the BVHPrimitves through the tree, allocate them all ahead of time and
    //  propegate the pointers (noderefs) to them. This also makes it easy to add transform nodes as leaves.
    std::vector<PrimitiveBuilder> primitives;
    Bounds3f realBounds;
    Bounds3f centerBounds;

    // TODO: parallelize and write this using STL algorithms?
    m_numPrimitives = 0;
    for (uint32_t geomID = 0; geomID < (uint32_t)geometry.size(); geomID++) {
        auto primBounds = geometry[geomID]->getPrimitivesBounds();
        primitives.reserve(primitives.size() + primBounds.size());

        for (uint32_t primID = 0; primID < (uint32_t)primBounds.size(); primID++) {
            m_numPrimitives++;

            auto bounds = primBounds[primID];
            realBounds.extend(bounds);
            centerBounds.grow(bounds.center());
            primitives.push_back({ bounds, bounds.center(), { geomID, primID } });
        }
    }

    std::cout << "NUM PRIMS: " << m_numPrimitives << std::endl;

    // Allocate a root node
    auto* rootNodePtr = m_bvh->m_primitiveAllocator.template allocate<typename BVH<N>::InternalNode>(1, 32);
    m_bvh->m_rootNode = BVH<2>::NodeRef(rootNodePtr);

    // Recursively split the node using the Surface Area Heuristic
    NodeBuilder nodeBuildInfo;
    nodeBuildInfo.nodePtr = rootNodePtr;
    nodeBuildInfo.realBounds = realBounds;
    nodeBuildInfo.centerBounds = centerBounds;
    nodeBuildInfo.primitives = gsl::make_span(primitives);
    recurse(nodeBuildInfo);

    testBvh();
}

struct SAHBin {
    SAHBin()
        : primitiveCount(0){};
    SAHBin(const Bounds3f& realBounds_, int primitiveCount_)
        : realBounds(realBounds_)
        , primitiveCount(primitiveCount_){};
    Bounds3f realBounds;
    int primitiveCount;
};

template <int N>
template <int numBins>
typename BVHBuilderSAH<N>::ObjectSplit BVHBuilderSAH<N>::findObjectSplit(const NodeBuilder& nodeInfo)
{
    float bestSahCost = std::numeric_limits<float>::max();
    ObjectSplit bestSplit;
    for (int axis = 0; axis < 3; axis++) {
        // Bounds of the primitive AABB centers (so not of the AABBs themselves)
        float minBound = nodeInfo.centerBounds.bounds_min[axis];
        float maxBound = nodeInfo.centerBounds.bounds_max[axis];
        float extent = maxBound - minBound;

        // Bin the primitives based on the center of their AABB
        std::array<SAHBin, numBins> bins;
        for (auto primAndBounds : nodeInfo.primitives) {
            Vec3f center = primAndBounds.boundsCenter;

            if (center[axis] < minBound || center[axis] > maxBound) {
                std::cout << "findObjectSplit -> node prim center bounds are incorrect" << std::endl;
                exit(1);
            }

            float binF = (center[axis] - minBound) / extent * numBins;
            int bin = std::clamp((int)binF, 0, numBins - 1);
            bins[bin].primitiveCount++;
            bins[bin].realBounds.extend(primAndBounds.primBounds);
        }

        // TODO: Handle the case where the centers of all AABBs fall in the same bin (will now lead to endless recursion)
        // Test every splitting plane
        for (int i = 1; i < numBins; i++) {
            // Combine all the bins left of the splitting plane
            SAHBin left = std::accumulate(bins.begin(), bins.begin() + i, SAHBin(), [](SAHBin accum, const auto& bin) -> SAHBin {
                return {
                    accum.realBounds.extended(bin.realBounds),
                    accum.primitiveCount + bin.primitiveCount
                };
            });

            // Combine all the bins left of the splitting plane
            SAHBin right = std::accumulate(bins.begin() + i, bins.end(), SAHBin(), [](SAHBin accum, const auto& bin) -> SAHBin {
                return {
                    accum.realBounds.extended(bin.realBounds),
                    accum.primitiveCount + bin.primitiveCount
                };
            });

            float binSahCost = left.realBounds.area() * left.primitiveCount + right.realBounds.area() * right.primitiveCount;
            if (binSahCost < bestSahCost) {
                //std::cout << "Left prim count: " << left.primitiveCount << ", right prim count: " << right.primitiveCount << std::endl;
                //std::cout << "Bin " << i << " has SAH cost of:" << binSahCost << std::endl;
                bestSplit.axis = axis;
                bestSplit.splitLocation = minBound + extent / numBins * i;
                bestSplit.leftBounds = left.realBounds;
                bestSplit.rightBounds = right.realBounds;
                bestSahCost = binSahCost;
            }
        }
    }

    if (bestSahCost == std::numeric_limits<float>::max()) {
        std::cout << "TODO: SAHBuilder - handle cases where an object split is worse than not splitting" << std::endl;
        exit(1);
    }

    return bestSplit;
}

template <int N>
void BVHBuilderSAH<N>::recurse(const NodeBuilder& nodeInfo)
{
    auto* nodePtr = nodeInfo.nodePtr;

    ObjectSplit split = findObjectSplit<32>(nodeInfo);
    auto splitIter = std::partition(nodeInfo.primitives.begin(), nodeInfo.primitives.end(),
        [&](const PrimitiveBuilder& primBuilder) {
            return primBuilder.boundsCenter[split.axis] < split.splitLocation;
        });

    // TODO: work with N != 2
    std::tuple<decltype(splitIter), decltype(splitIter)> splitPrimitiveBuilders[2] = {
        { nodeInfo.primitives.begin(), splitIter },
        { splitIter, nodeInfo.primitives.end() }
    };
    Bounds3f splitBounds[2] = {
        split.leftBounds,
        split.rightBounds
    };
    for (int childIdx = 0; childIdx < 2; childIdx++) {
        nodePtr->childBounds[childIdx] = splitBounds[childIdx];

        auto [primBuilderBegin, primBuilderEnd] = splitPrimitiveBuilders[childIdx];
        size_t numPrims = primBuilderEnd - primBuilderBegin;
        if (numPrims < 4) {
            // Allocate space for the primitives
            auto* primitives = m_bvh->m_primitiveAllocator.template allocate<BvhPrimitive>(numPrims, 32);

            // Copy the primitves to the memory we just allocated
            std::transform(primBuilderBegin, primBuilderEnd, primitives, [](const PrimitiveBuilder& primBuilder) {
                return primBuilder.primitive;
            });

            nodePtr->children[childIdx] = typename BVH<N>::NodeRef(primitives, numPrims);
        } else {
            // Allocate a new inner node and assign it as a child
            auto* childNode = m_bvh->m_primitiveAllocator.template allocate<typename BVH<N>::InternalNode>(1, 32);
            nodePtr->children[childIdx] = typename BVH<N>::NodeRef(childNode);

            // Calculate the bounds of the primitives AABB centers
            Bounds3f childCenterBounds;
            for (auto iter = primBuilderBegin; iter < primBuilderEnd; iter++)
                childCenterBounds.grow(iter->boundsCenter);

            // Seems like gsl::span does not like iterators and it feells the need to know the underlying data structure
            size_t primStartIdx = primBuilderBegin - nodeInfo.primitives.begin();
            size_t primEndIdx = primBuilderEnd - nodeInfo.primitives.begin();

            NodeBuilder childBuilder;
            childBuilder.nodePtr = childNode;
            childBuilder.realBounds = splitBounds[childIdx];
            childBuilder.centerBounds = childCenterBounds;
            childBuilder.primitives = nodeInfo.primitives.subspan(primStartIdx, primEndIdx - primStartIdx);
            recurse(childBuilder);
        }
    }
}

template <int N>
void BVHBuilderSAH<N>::testBvh()
{
    // Test that we can reach all the primitives
    // TODO: gsl_span gives a bunch of compile errors when unordered_map is included so use std::map for now
    std::unordered_map<uint64_t, bool> reachablePrims;

    std::vector<BVH<2>::NodeRef> traversalStack = { m_bvh->m_rootNode };
    while (!traversalStack.empty()) {
        auto nodeRef = traversalStack.back();
        traversalStack.pop_back();

        if (nodeRef.isInternalNode()) {
            auto* nodePtr = nodeRef.getInternalNode();
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

    int reachable = std::accumulate(std::begin(reachablePrims), std::end(reachablePrims), 0,
        [](int counter, const decltype(reachablePrims)::value_type& p) {
            return counter + p.second;
        });
    std::cout << reachable << " out of " << m_numPrimitives << " primitives reachable" << std::endl;
}

template class BVHBuilderSAH<2>;
}
