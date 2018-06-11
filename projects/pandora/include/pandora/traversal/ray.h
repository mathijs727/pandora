#pragma once
#include "glm/glm.hpp"
#include "glm/glm.hpp"
#include <limits>

namespace pandora {

class Shape;

struct Ray {
public:
    Ray() = default;
    Ray(glm::vec3 origin_, glm::vec3 direction_)
        : origin(origin_)
        , direction(direction_)
		, tnear(0.0f)
		, tfar(std::numeric_limits<float>::max())
	{
	}

    glm::vec3 origin;
    glm::vec3 direction;
	float tnear;
	float tfar;
};

struct IntersectionData {
	const Shape* objectHit;

	glm::vec2 uv;
	glm::vec3 geometricNormal;
};

}