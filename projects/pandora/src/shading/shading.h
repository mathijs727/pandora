#pragma once
#include "glm/glm.hpp"

namespace pandora {

glm::vec3 uniformSampleHemisphere(glm::vec3 normal);

template <typename T>
constexpr T pi();

template <>
constexpr float pi()
{
    return 3.1415927f;
}

template <>
constexpr double pi()
{
    return 3.141592653589793;
}

}
