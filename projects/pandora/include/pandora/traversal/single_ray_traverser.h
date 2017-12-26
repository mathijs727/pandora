#pragma once
#include "pandora/traversal/bvh.h"
#include "pandora/traversal/ray.h"

namespace pandora {

class SingleRayTraverser {
public:
    SingleRayTraverser(BVH<2>& bvh);

    bool intersect(Ray& ray);

private:
    BVH<2>& m_bvh;
};
}