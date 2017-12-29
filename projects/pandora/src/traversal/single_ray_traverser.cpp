#include "pandora/traversal/single_ray_traverser.h"
#include "pandora/utility/fixed_stack.h"
#include <iostream>

namespace pandora {

SingleRayTraverser::SingleRayTraverser(BVH<2>& bvh)
    : m_bvh(bvh)
{
}

bool SingleRayTraverser::intersect(Ray& ray)
{
    IntersectionInfo intersectInfo = {};
    return intersectOptionalInfo<false>(ray, intersectInfo);
}

bool SingleRayTraverser::intersect(Ray& ray, IntersectionInfo& info)
{
    return intersectOptionalInfo<true>(ray, info);
}

template <bool intersectionInfo>
bool SingleRayTraverser::intersectOptionalInfo(Ray& ray, IntersectionInfo& info)
{
    fixed_stack<BVH<2>::NodeRef, 64> traversalStack;
    traversalStack.push(m_bvh.m_rootNode);

    bool hit = false;
    while (!traversalStack.empty()) {
        auto nodeRef = traversalStack.pop();

        if (nodeRef.isInternalNode()) {
            auto* nodePtr = nodeRef.getInternalNode();

            // Is inner node
            bool intersectsLeft, intersectsRight;
            float tminLeft, tminRight, tmaxLeft, tmaxRight;
            intersectsLeft = nodePtr->childBounds[0].intersect(ray, tminLeft, tmaxLeft);
            intersectsRight = nodePtr->childBounds[1].intersect(ray, tminRight, tmaxRight);

            if (tminLeft > ray.t)
                intersectsLeft = false;

            if (tminRight > ray.t)
                intersectsRight = false;

            if (intersectsLeft && intersectsRight) {
                // Both hit -> ordered traversal
                if (tminLeft < tminRight) {
                    traversalStack.push(nodePtr->children[1]);
                    traversalStack.push(nodePtr->children[0]); // Left traversed first
                } else {
                    traversalStack.push(nodePtr->children[0]);
                    traversalStack.push(nodePtr->children[1]);
                }
            } else if (intersectsLeft) {
                traversalStack.push(nodePtr->children[0]);
            } else if (intersectsRight) {
                traversalStack.push(nodePtr->children[1]);
            }
        } else if (nodeRef.isLeaf()) {
            auto* primitives = nodeRef.getPrimitives();

            // Node refers to one or more primitives
            for (uint64_t i = 0; i < nodeRef.numPrimitives(); i++) {
                auto [geomID, primID] = primitives[i];
                hit |= m_bvh.m_shapes[geomID]->intersect(primID, ray);

                if constexpr (intersectionInfo)
                    info.numPrimIntersectTests++;
            }
        } // TODO: transform nodes
    }

    return hit;
}
}