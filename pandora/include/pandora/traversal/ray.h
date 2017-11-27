#pragma once
#include "pandora/math/vec3.h"

namespace pandora {

struct Ray {
public:
    Ray() = default;
    Ray(Vec3f origin_, Vec3f direction_)
        : origin(origin_)
        , direction(direction_)
        , t(0.0f){};

    Vec3f origin;
    Vec3f direction;
    float t;
};

struct ShadeData {
    // The object that we hit
    float u, v;
};
}