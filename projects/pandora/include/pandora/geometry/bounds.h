#pragma once
#include "glm/glm.hpp"
#include "pandora/core/ray.h"

namespace pandora {

struct Bounds {
public:
    Bounds();
    Bounds(glm::vec3 lower, glm::vec3 upper);

	glm::vec3 getExtent() const;

    void reset();
    void grow(glm::vec3 vec);
    void extend(const Bounds& other);
    Bounds extended(const Bounds& other) const;

    glm::vec3 center() const;
    float surfaceArea() const;

    bool intersect(const Ray& ray, float& tmin, float& tmax) const;

public:
    glm::vec3 min, max;
};

}
