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
inline uint32_t floatToBits(float f)
{
    uint32_t ui;
    memcpy(&ui, &f, sizeof(float));
    return ui;
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float bitsTofloat(uint32_t ui)
{
    float f;
    memcpy(&f, &ui, sizeof(uint32_t));
    return f;
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float nextfloatUp(float v)
{
    // Handle infinity and negative zero for _NextfloatUp()_
    if (std::isinf(v) && v > 0.)
        return v;
    if (v == -0.f)
        v = 0.f;

    // Advance _v_ to next higher float
    uint32_t ui = floatToBits(v);
    if (v >= 0)
        ++ui;
    else
        --ui;
    return bitsTofloat(ui);
}

// https://github.com/mmp/pbrt-v3/blob/book/src/core/pbrt.h
inline float nextfloatDown(float v)
{
    // Handle infinity and positive zero for _NextfloatDown()_
    if (std::isinf(v) && v < 0.)
        return v;
    if (v == 0.f)
        v = -0.f;
    uint32_t ui = floatToBits(v);
    if (v > 0)
        --ui;
    else
        ++ui;
    return bitsTofloat(ui);
}

// PBRTv3 page 66
inline float lengthSquared(const glm::vec3& v)
{
    return glm::dot(v, v);
}

inline float distance(const glm::vec3& p0, const glm::vec3& p1)
{
    return glm::length(p0 - p1);
}

inline float distanceSquared(const glm::vec3& p0, const glm::vec3& p1)
{
    return lengthSquared(p0 - p1);
}

inline float absDot(const glm::vec3& v0, const glm::vec3& v1)
{
    return glm::abs(glm::dot(v0, v1));
}

inline float minComponent(const glm::vec3& v)
{
    return std::min(v.x, std::min(v.y, v.z));
}

inline float maxComponent(const glm::vec3& v)
{
    return std::max(v.x, std::max(v.y, v.z));
}

inline int maxDimension(const glm::vec3& v)
{
    if (v.x > v.y) {
		if (v.x > v.z)
			return 0;
		else
			return 2;
    } else {
		if (v.y > v.z)
			return 1;
		else
			return 2;
    }
}

inline glm::vec3 permute(const glm::vec3& p, int x, int y, int z)
{
    return glm::vec3(p[x], p[y], p[z]);
}

// https://github.com/mmp/pbrt-v3/blob/aaecc9112522cf8a791a3ecb5e3efe716ce30793/src/core/pbrt.h
inline float erfInv(float x) {
	float w, p;
	x = std::clamp(x, -.99999f, .99999f);
	w = -std::log((1 - x) * (1 + x));
	if (w < 5) {
		w = w - 2.5f;
		p = 2.81022636e-08f;
		p = 3.43273939e-07f + p * w;
		p = -3.5233877e-06f + p * w;
		p = -4.39150654e-06f + p * w;
		p = 0.00021858087f + p * w;
		p = -0.00125372503f + p * w;
		p = -0.00417768164f + p * w;
		p = 0.246640727f + p * w;
		p = 1.50140941f + p * w;
	}
	else {
		w = std::sqrt(w) - 3;
		p = -0.000200214257f;
		p = 0.000100950558f + p * w;
		p = 0.00134934322f + p * w;
		p = -0.00367342844f + p * w;
		p = 0.00573950773f + p * w;
		p = -0.0076224613f + p * w;
		p = 0.00943887047f + p * w;
		p = 1.00167406f + p * w;
		p = 2.83297682f + p * w;
	}
	return p * x;
}

// https://github.com/mmp/pbrt-v3/blob/aaecc9112522cf8a791a3ecb5e3efe716ce30793/src/core/pbrt.h
inline float erf(float x) {
	// constants
	float a1 = 0.254829592f;
	float a2 = -0.284496736f;
	float a3 = 1.421413741f;
	float a4 = -1.453152027f;
	float a5 = 1.061405429f;
	float p = 0.3275911f;

	// Save the sign of x
	int sign = 1;
	if (x < 0) sign = -1;
	x = std::abs(x);

	// A&S formula 7.1.26
	float t = 1 / (1 + p * x);
	float y =
		1 -
		(((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);

	return sign * y;
}

}
