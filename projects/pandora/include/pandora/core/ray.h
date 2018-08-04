#pragma once
#include "glm/glm.hpp"
#include "pandora/core/pandora.h"
#include <limits>

namespace pandora {

const float RAY_EPSILON = 0.000005f;

struct Ray {
public:
    Ray() = default;
    Ray(const glm::vec3& origin, const glm::vec3& direction)
        : origin(origin)
        , direction(direction)
        , tnear(0.0f)
        , tfar(std::numeric_limits<float>::max())
    {
    }
     Ray(const glm::vec3& origin, const glm::vec3& direction, float tnear, float tfar = std::numeric_limits<float>::max())
        : origin(origin)
        , direction(direction)
        , tnear(tnear)
        , tfar(tfar)
    {
    }


    glm::vec3 origin;
    glm::vec3 direction;
    float tnear;
    float tfar;
};

template <int N>
struct vec3SOA
{
	float x[N];
	float y[N];
	float z[N];
};

template <int N>
struct RaySOA
{
	vec3SOA<N> origin;
	vec3SOA<N> direction;
	float tnear[N];
	float tfar[N];
};

}
