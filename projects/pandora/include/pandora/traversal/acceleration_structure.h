#pragma once
#include "pandora/graphics_core/pandora.h"

namespace pandora {

template <typename HitRayState, typename AnyHitRayState>
class AccelerationStructure {
public:
    virtual void intersect(const Ray& ray, const HitRayState& state) const = 0;
    virtual void intersectAny(const Ray& ray, const AnyHitRayState& state) const = 0;
};

}