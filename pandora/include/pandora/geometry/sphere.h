#pragma once
#include "pandora/math/vec3.h"

namespace pandora {

struct Sphere {
public:
    Sphere(Vec3f center_, float radius_)
        : center(center_)
        , radius(radius_)
        , radius2(radius_ * radius_)
    {
    }

    Vec3f center;
    float radius, radius2;
};
}