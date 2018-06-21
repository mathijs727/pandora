#pragma once
#include "glm/glm.hpp"
#include <algorithm>

namespace pandora {

// These functions assume that they values are in the shading coordinate system (normal orientated along z-axis)
inline glm::vec3 faceForward(const glm::vec3& n, const glm::vec3& v)
{
    return (glm::dot(n, v) < 0.0f) ? -n : n;
}

inline void coordinateSystem(const glm::vec3& v1, glm::vec3* v2, glm::vec3* v3)
{
    if (std::abs(v1.x) > std::abs(v1.y))
        *v2 = glm::vec3(-v1.z, 0, v1.x) / std::sqrt(v1.x * v1.x + v1.z * v1.z);
    else
        *v2 = glm::vec3(0, v1.z, -v1.y) / std::sqrt(v1.y * v1.y + v1.z * v1.z);
    *v3 = glm::cross(v1, *v2);
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline uint32_t floatToBits(float f) {
    uint32_t ui;
    memcpy(&ui, &f, sizeof(float));
    return ui;
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float bitsToFloat(uint32_t ui) {
    float f;
    memcpy(&f, &ui, sizeof(uint32_t));
    return f;
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float nextFloatUp(float v) {
    // Handle infinity and negative zero for _NextFloatUp()_
    if (std::isinf(v) && v > 0.) return v;
    if (v == -0.f) v = 0.f;

    // Advance _v_ to next higher float
    uint32_t ui = floatToBits(v);
    if (v >= 0)
        ++ui;
    else
        --ui;
    return bitsToFloat(ui);
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float nextFloatDown(float v) {
    // Handle infinity and positive zero for _NextFloatDown()_
    if (std::isinf(v) && v < 0.) return v;
    if (v == 0.f) v = -0.f;
    uint32_t ui = floatToBits(v);
    if (v > 0)
        --ui;
    else
        ++ui;
    return bitsToFloat(ui);
}


}
