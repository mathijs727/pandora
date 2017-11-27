#pragma once
#include "pandora/math/vec3.h"

namespace pandora {

struct Sphere {
public:
    Sphere(Vec3f center, float radius)
        : m_center(center)
        , m_radius(radius)
    {
    }

    Vec3f m_center;
    float m_radius;
};
}