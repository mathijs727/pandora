#pragma once
#include "glm/glm.hpp"

namespace pandora {

struct Sphere {
public:
    Sphere(glm::vec3 center_, float radius_)
        : center(center_)
        , radius(radius_)
        , radius2(radius_ * radius_)
    {
    }

    glm::vec3 center;
    float radius, radius2;
};
}