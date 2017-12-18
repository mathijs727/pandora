#pragma once
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
#include <limits>

namespace pandora {

struct Ray {
public:
    Ray() = default;
    Ray(Vec3f origin_, Vec3f direction_)
        : origin(origin_)
        , direction(direction_)
        , t(std::numeric_limits<float>::max()){};

    Vec3f origin;
    Vec3f direction;
    Vec2f uv;
    float t;
};

struct ShadeData {
    //TODO(Mathijs): Pointer to the object that we hit
    Vec3f hitPoint;
    Vec3f normal;
};
}