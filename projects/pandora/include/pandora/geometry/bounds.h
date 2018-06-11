#pragma once
#include "glm/glm.hpp"
#include "pandora/traversal/ray.h"

namespace pandora {

struct Bounds {
public:
    Bounds();
    Bounds(glm::vec3 lower, glm::vec3 upper);

    void reset();
    void grow(glm::vec3 vec);
    void extend(const Bounds& other);
    Bounds extended(const Bounds& other) const;

    glm::vec3 center() const;
    float area() const;
    float halfArea() const;

    bool intersect(const Ray& ray, float& tmin, float& tmax);

public:
    glm::vec3 min, max;
};

}