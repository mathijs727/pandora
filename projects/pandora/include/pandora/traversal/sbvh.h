#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/traversal/ray.h"
#include <tuple>
#include <vector>

namespace pandora {

struct BvhNode { // 32 bytes
    Bounds3f bounds; // 24 bytes
    union // 4 bytes
    {
        uint32_t leftChild; // Inner
        uint32_t firstPrimitive; // Leaf
    };
    uint32_t primitiveCount; // 4 bytes
};

class TwoLevelSbvhAccel {
public:
    TwoLevelSbvhAccel(const std::vector<const Shape*>& geometry);

    bool intersect(Ray& ray);

private:
    template <bool botLvlTraversal>
    bool intersect(uint32_t startIndex, Ray& ray);

    uint32_t buildBvh(const std::vector<const Shape*>& geometry);

    std::vector<BvhNode> buildBotBvh(const Shape*);

    void partition(
        uint32_t nodeIndex,
        uint32_t leftChild,
        uint32_t rightChild,
        std::vector<std::tuple<Bounds3f, uint32_t>>& allPrimitives,
        std::vector<BvhNode>& bvhNodes);
    void subdivide(
        uint32_t nodeIndex,
        std::vector<std::tuple<Bounds3f, uint32_t>>& allPrimitives,
        std::vector<BvhNode>& bvhNodes);

    void testBvh();

private:
    const Shape* m_myShape;
    uint32_t m_rootNode;

    std::vector<uint32_t> m_primitivesIndices;
    std::vector<BvhNode> m_topBvhNodes;
    std::vector<BvhNode> m_botBvhNodes;
};
}