#pragma once
#include "pandora/math/bounds3.h"
#include "pandora/math/vec2.h"
#include "pandora/traversal/ray.h"

#include <gsl/gsl>

namespace pandora {
class Shape {
public:
    virtual unsigned numPrimitives() const = 0;
    virtual gsl::span<const Bounds3f> getPrimitivesBounds() const = 0;

    virtual bool intersect(unsigned primitiveIndex, Ray& ray) const = 0;
    virtual Vec3f getNormal(unsigned primitiveIndex, Vec2f uv) const = 0;

    //virtual unsigned addToEmbreeScene(RTCScene& scene) const = 0;
};
}