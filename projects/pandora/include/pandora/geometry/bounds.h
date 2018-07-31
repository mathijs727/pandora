#pragma once
#include "glm/glm.hpp"
#include "pandora/core/ray.h"

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
	glm::vec3 extent() const;
    float surfaceArea() const;

	Bounds intersection(const Bounds& other) const;
	bool overlaps(const Bounds& other) const;
    bool intersect(const Ray& ray, float& tmin, float& tmax) const;

public:
    glm::vec3 min, max;
};

}
