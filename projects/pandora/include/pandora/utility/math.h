#pragma once
#include "glm/glm.hpp"
#include <algorithm>

namespace pandora {

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

}
