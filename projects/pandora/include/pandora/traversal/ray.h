#pragma once
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
#include <limits>

namespace pandora {

class Shape;

struct Ray {
public:
    Ray() = default;
    Ray(Vec3f origin_, Vec3f direction_)
        : origin(origin_)
        , direction(direction_)
		, tnear(0.0f)
		, tfar(std::numeric_limits<float>::max())
	{
	}

    Vec3f origin;
    Vec3f direction;
	float tnear;
	float tfar;
};

struct IntersectionData {
	const Shape* objectHit;

	Vec2f uv;
	Vec3f geometricNormal;
};

}