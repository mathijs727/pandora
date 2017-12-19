#include "pandora/traversal/sbvh.h"
#include <algorithm>
#include <iostream>

namespace pandora {

TwoLevelSbvhAccel::TwoLevelSbvhAccel(const std::vector<const Shape*>& geometry)
{
    m_rootNode = buildBvh(geometry);
}

bool TwoLevelSbvhAccel::intersect(Ray& ray)
{
    std::vector<uint32_t> traversalStack = { 0 };
    bool hit = false;

    while (!traversalStack.empty()) {
        auto node = m_botBvhNodes[traversalStack.back()];
        traversalStack.pop_back();

        if (node.primitiveCount > 0) {
            // Is leaf
            for (uint32_t i = 0; i < node.primitiveCount; i++) {
                uint32_t primIdx = m_primitivesIndices[node.firstPrimitive + i];
                hit |= m_myShape->intersect(primIdx, ray);
            }
        } else {
            // Is inner node
            uint32_t leftChildIdx = node.leftChild;
            uint32_t rightChildIdx = node.leftChild + 1;

            bool intersectsLeft, intersectsRight;
            float tminLeft, tminRight, tmaxLeft, tmaxRight;
            intersectsLeft = m_botBvhNodes[leftChildIdx].bounds.intersect(ray, tminLeft, tmaxLeft);
            intersectsRight = m_botBvhNodes[rightChildIdx].bounds.intersect(ray, tminRight, tmaxRight);

            if (tminLeft > ray.t)
                intersectsLeft = false;

            if (tminRight > ray.t)
                intersectsRight = false;

            if (intersectsLeft && intersectsRight) {
                // Both hit -> ordered traversal
                if (tminLeft < tminRight) {
                    traversalStack.push_back(leftChildIdx);
                    traversalStack.push_back(rightChildIdx);
                } else {
                    traversalStack.push_back(rightChildIdx);
                    traversalStack.push_back(leftChildIdx);
                }
            } else if (intersectsLeft) {
                traversalStack.push_back(leftChildIdx);
            } else if (intersectsRight) {
                traversalStack.push_back(rightChildIdx);
            }
        }
    }

    return hit;
}

uint32_t TwoLevelSbvhAccel::buildBvh(const std::vector<const Shape*>& geometry)
{
    std::vector<std::tuple<uint32_t, Bounds3f>> shapeBounds;
    //for (const Shape* shape : geometry) {
    {
        const Shape* shape = geometry[0];
        m_myShape = shape;

        auto shapesBotBvhNodes = buildBotBvh(shape);
        uint32_t rootNode = (uint32_t)m_topBvhNodes.size(); // Assume root node is the first node in the vector
        m_botBvhNodes.insert(std::end(m_botBvhNodes), begin(shapesBotBvhNodes), std::end(shapesBotBvhNodes));

        shapeBounds.push_back({ rootNode, shapesBotBvhNodes[0].bounds });
    }

    return 0;
}

void TwoLevelSbvhAccel::partition(
    uint32_t nodeIndex,
    uint32_t leftChildIdx,
    uint32_t rightChildIdx,
    std::vector<std::tuple<Bounds3f, uint32_t>>& allPrimitives,
    std::vector<BotBvhNode>& bvhNodes)
{
    // Use indices so we dont have a problem with pointers getting free'd when the vector we allocate
    // from decides it needs to reallocate to grow.
    auto& node = bvhNodes[nodeIndex];

    // Split based on the largest dimension
    int splitAxis = maxDimension(node.bounds.bounds_max - node.bounds.bounds_min);

    const auto startPrims = std::begin(allPrimitives) + node.firstPrimitive;
    const auto endPrims = startPrims + node.primitiveCount;
    std::sort(startPrims, endPrims, [&](const auto& left, const auto& right) {
        const auto& [boundsLeft, primIdxLeft] = left;
        const auto& [boundsRight, primIdxRight] = right;

        auto leftAabbCenter = boundsLeft.bounds_max[splitAxis] - boundsLeft.bounds_min[splitAxis];
        auto rightAabbCenter = boundsRight.bounds_max[splitAxis] - boundsRight.bounds_min[splitAxis];

        return leftAabbCenter < rightAabbCenter;
    });

    uint32_t leftPrimCount = node.primitiveCount / 2;

    auto& leftNode = bvhNodes[leftChildIdx];
    leftNode.primitiveCount = leftPrimCount;
    leftNode.firstPrimitive = node.firstPrimitive;
    leftNode.bounds = std::accumulate(
        startPrims,
        startPrims + leftPrimCount,
        Bounds3f(),
        [&](Bounds3f boundsLeft, const std::tuple<Bounds3f, uint32_t>& right) {
            auto [boundsRight, primIdxRight] = right;

            boundsLeft.merge(boundsRight);
            return boundsLeft;
        });

    auto& rightNode = bvhNodes[rightChildIdx];
    rightNode.primitiveCount = node.primitiveCount - leftPrimCount;
    rightNode.firstPrimitive = node.firstPrimitive + leftPrimCount;
    rightNode.bounds = std::accumulate(
        startPrims + leftPrimCount,
        endPrims,
        Bounds3f(),
        [&](Bounds3f boundsLeft, const std::tuple<Bounds3f, uint32_t>& right) {
            auto [boundsRight, primIdxRight] = right;

            boundsLeft.merge(boundsRight);
            return boundsLeft;
        });

    node.leftChild = leftChildIdx;
    node.primitiveCount = 0;
}

void TwoLevelSbvhAccel::subdivide(
    uint32_t nodeIndex,
    std::vector<std::tuple<Bounds3f, uint32_t>>& allPrimitives,
    std::vector<BotBvhNode>& bvhNodes)
{
    auto& node = bvhNodes[nodeIndex];
    if (node.primitiveCount < 4)
        return;

    uint32_t leftChildIdx = bvhNodes.size();
    bvhNodes.emplace_back();

    uint32_t rightChildIdx = bvhNodes.size();
    bvhNodes.emplace_back();

    partition(nodeIndex, leftChildIdx, rightChildIdx, allPrimitives, bvhNodes);
    subdivide(leftChildIdx, allPrimitives, bvhNodes);
    subdivide(rightChildIdx, allPrimitives, bvhNodes);
}

std::vector<BotBvhNode> TwoLevelSbvhAccel::buildBotBvh(const Shape* shape)
{

    auto bounds = shape->getPrimitivesBounds();

    // Make tuples of bounding boxes and the corresponding primitive indices
    std::vector<std::tuple<Bounds3f, uint32_t>> boundsAndPrims(bounds.size());
    uint32_t i = 0;
    std::transform(std::begin(bounds), std::end(bounds), std::begin(boundsAndPrims), [&](const auto& bounds) {
        return std::make_tuple(bounds, i++);
    });

    // Initialize root node
    std::vector<BotBvhNode> nodes(2); // Start with 2 for cache line alignment
    nodes[0].firstPrimitive = 0;
    nodes[0].primitiveCount = (uint32_t)bounds.size();
    nodes[0].bounds = std::accumulate(std::begin(bounds), std::end(bounds), Bounds3f(),
        [](const Bounds3f& left, const Bounds3f& right) {
            Bounds3f result = left;
            result.merge(right);
            return result;
        });

    // Build the BVH
    subdivide(0, boundsAndPrims, nodes);

    // The subdivide BVH has changed the order of the primitives.
    // Instead of changing the order in the Shape structure (which is very intrusive), we add an extra layer
    // of indirection.
    m_primitivesIndices.resize(boundsAndPrims.size());
    for (uint32_t i = 0; i < (uint32_t)boundsAndPrims.size(); i++) {
        m_primitivesIndices[i] = std::get<1>(boundsAndPrims[i]);
    }

    /*// Test that we can reach all the primitives
    std::vector<bool> reachablePrims(boundsAndPrims.size());

    std::vector<uint32_t> traversalStack = { 0 };
    while (!traversalStack.empty()) {
        auto nodeIndex = traversalStack.back();
        auto node = nodes[nodeIndex];
        traversalStack.pop_back();

        if (node.primitiveCount > 0) {
            // Is leaf
            for (uint32_t i = 0; i < node.primitiveCount; i++) {
                uint32_t primIdx = m_primitivesIndices[node.firstPrimitive + i];
                reachablePrims[primIdx] = true;


                auto [primBounds, actualPrimIdx] = boundsAndPrims[node.firstPrimitive + i];
                if ((primBounds.bounds_min.x < node.bounds.bounds_min.x) ||
                    (primBounds.bounds_min.y < node.bounds.bounds_min.y) ||
                    (primBounds.bounds_min.z < node.bounds.bounds_min.z) ||
                    (primBounds.bounds_max.x > node.bounds.bounds_max.x) ||
                    (primBounds.bounds_max.y > node.bounds.bounds_max.y) ||
                    (primBounds.bounds_max.z > node.bounds.bounds_max.z)) {
                    std::cout << "Primitive not fully contained in parent node" << std::endl;
                }

                if (actualPrimIdx != primIdx)
                {
                    std::cout << "Primitive look-up broken" << std::endl;
                }
            }
        } else {
            // Is inner node
            uint32_t leftChildIdx = node.leftChild;
            uint32_t rightChildIdx = node.leftChild + 1;

            // Check that child bounds are strictly smaller than parent bounds
            auto leftChild = nodes[leftChildIdx];
            if ((leftChild.bounds.bounds_min.x < node.bounds.bounds_min.x) || (leftChild.bounds.bounds_min.y < node.bounds.bounds_min.y) || (leftChild.bounds.bounds_min.z < node.bounds.bounds_min.z) || (leftChild.bounds.bounds_max.x > node.bounds.bounds_max.x) || (leftChild.bounds.bounds_max.y > node.bounds.bounds_max.y) || (leftChild.bounds.bounds_max.z > node.bounds.bounds_max.z)) {
                std::cout << "Left child node not fully contained in parent node" << std::endl;
            }

            auto rightChild = nodes[rightChildIdx];
            if ((rightChild.bounds.bounds_min.x < node.bounds.bounds_min.x) || (rightChild.bounds.bounds_min.y < node.bounds.bounds_min.y) || (rightChild.bounds.bounds_min.z < node.bounds.bounds_min.z) || (rightChild.bounds.bounds_max.x > node.bounds.bounds_max.x) || (rightChild.bounds.bounds_max.y > node.bounds.bounds_max.y) || (rightChild.bounds.bounds_max.z > node.bounds.bounds_max.z)) {
                std::cout << "Left child node not fully contained in parent node" << std::endl;
            }

            traversalStack.push_back(leftChildIdx);
            traversalStack.push_back(rightChildIdx);
        }
    }

    int reachable = std::accumulate(std::begin(reachablePrims), std::end(reachablePrims), 0);
    std::cout << reachable << " out of " << boundsAndPrims.size() << " primitives reachable" << std::endl;*/

    return nodes;
}
}