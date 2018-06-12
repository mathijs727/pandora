#pragma once
#include "pandora/traversal/ray.h"
#include <gsl/span>

namespace pandora {

class AccelerationStructure {
public:
    virtual void intersect(Ray& ray, IntersectionData& intersectionData) = 0;
    //virtual void intersect(gsl::span<Ray> rays, gsl::span<IntersectionData> intersectionData) = 0;
};

}
