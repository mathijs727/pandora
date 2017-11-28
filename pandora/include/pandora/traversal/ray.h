#pragma once
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
    float t;
};

struct ShadeData {
    // The object that we hit
    float u, v;
};
}