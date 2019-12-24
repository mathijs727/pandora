#pragma once
#include "glm/glm.hpp"
#include "pandora/graphics_core/ray.h"
#include "pandora/flatbuffers/data_types_generated.h"

struct RTCBounds;

namespace pandora {

struct Bounds {
public:
    Bounds();
    Bounds(glm::vec3 lower, glm::vec3 upper);
    Bounds(const RTCBounds& embreeBounds);
    Bounds(const serialization::Bounds& serialized);

	Bounds& operator*=(const glm::mat4& matrix);
    bool operator==(const Bounds& other) const;

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
    bool contains(const glm::vec3& point) const;
    bool contains(const Bounds& other) const;

    serialization::Bounds serialize() const;

public:
    glm::vec3 min, max;
};

}
