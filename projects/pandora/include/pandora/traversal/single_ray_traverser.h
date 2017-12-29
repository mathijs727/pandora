#pragma once
#include "pandora/traversal/bvh.h"
#include "pandora/traversal/ray.h"
#include <vector>

namespace pandora {

struct IntersectionInfo {
    int numPrimIntersectTests = 0;
};

class SingleRayTraverser {
public:
    SingleRayTraverser(BVH<2>& bvh);

    bool intersect(Ray& ray);
    bool intersect(Ray& ray, IntersectionInfo& info);

private:
    template <bool intersectionInfo>
    bool intersectOptionalInfo(Ray& ray, IntersectionInfo& info);

private:
    BVH<2>& m_bvh;
};
}