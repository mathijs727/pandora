#include "reflection/microfacet.h"
#include "glm/gtc/constants.hpp"
#include "pandora/utility/math.h"
#include "reflection/helpers.h"
#include <algorithm>

namespace pandora {

// https://github.com/mmp/pbrt-v3/blob/master/src/core/microfacet.cpp
static glm::vec2 beckmannSample11(float cosThetaI, float u1, float u2)
{
    /* Special case (normal incidence) */
    if (cosThetaI > .9999) {
        float r = std::sqrt(-std::log(1.0f - u1));
        float sinPhi = std::sin(2 * glm::pi<float>() * u2);
        float cosPhi = std::cos(2 * glm::pi<float>() * u2);
        return glm::vec2(r * cosPhi, r * sinPhi);
    }

    /* The original inversion routine from the paper contained
	discontinuities, which causes issues for QMC integration
	and techniques like Kelemen-style MLT. The following code
	performs a numerical inversion with better behavior */
    float sinThetaI = std::sqrt(std::max((float)0, (float)1 - cosThetaI * cosThetaI));
    float tanThetaI = sinThetaI / cosThetaI;
    float cotThetaI = 1 / tanThetaI;

    /* Search interval -- everything is parameterized
	in the Erf() domain */
    float a = -1, c = erf(cotThetaI);
    float sampleX = std::max(u1, (float)1e-6f);

    /* Start with a good initial guess */
    // float b = (1-sample_x) * a + sample_x * c;

    /* We can do better (inverse of an approximation computed in
	* Mathematica) */
    float thetaI = std::acos(cosThetaI);
    float fit = 1 + thetaI * (-0.876f + thetaI * (0.4265f - 0.0594f * thetaI));
    float b = c - (1 + c) * std::pow(1 - sampleX, fit);

    /* Normalization factor for the CDF */
    static const float SQRT_PI_INV = 1.f / std::sqrt(glm::pi<float>());
    float normalization = 1 / (1 + c + SQRT_PI_INV * tanThetaI * std::exp(-cotThetaI * cotThetaI));

    int it = 0;
    while (++it < 10) {
        /* Bisection criterion -- the oddly-looking
		Boolean expression are intentional to check
		for NaNs at little additional cost */
        if (!(b >= a && b <= c))
            b = 0.5f * (a + c);

        /* Evaluate the CDF and its derivative
		(i.e. the density function) */
        float invErf = erfInv(b);
        float value = normalization * (1 + b + SQRT_PI_INV * tanThetaI * std::exp(-invErf * invErf)) - sampleX;
        float derivative = normalization * (1 - invErf * tanThetaI);

        if (std::abs(value) < 1e-5f)
            break;

        /* Update bisection intervals */
        if (value > 0)
            c = b;
        else
            a = b;

        b -= value / derivative;
    }

    /* Now convert back into a slope value */
    //slope.x = erfInv(b);

    /* Simulate Y component */
    //slope.y = erfInv(2.0f * std::max(u2, (float)1e-6f) - 1.0f);

    return glm::vec2(erfInv(b), erfInv(2.0f * std::max(u2, (float)1e-6f) - 1.0f));
}

// https://github.com/mmp/pbrt-v3/blob/master/src/core/microfacet.cpp
static glm::vec3 beckmannSample(const glm::vec3& wi, float alphaX, float alphaY, float u1, float u2)
{
    // 1. Stretch wi
    glm::vec3 wiStretched = glm::normalize(glm::vec3(alphaX * wi.x, alphaY * wi.y, wi.z));

    // 2. Simulate p22_{wi}(x_slope, y_slope, 1, 1)
    glm::vec2 slope = beckmannSample11(cosTheta(wiStretched), u1, u2);

    // 3. Rotate
    float tmp = cosPhi(wiStretched) * slope.x - sinPhi(wiStretched) * slope.y;
    slope.y = sinPhi(wiStretched) * slope.x + cosPhi(wiStretched) * slope.y;
    slope.x = tmp;

    // 4. Unstretch
    slope.x = alphaX * slope.x;
    slope.y = alphaY * slope.y;

    // 5. Compute normal;
    return glm::normalize(glm::vec3(-slope.x, -slope.y, 1.0f));
}

float MicrofacetDistribution::pdf(const glm::vec3& wo, const glm::vec3& wh) const
{
    return D(wh) * G1(wo) * absDot(wo, wh) / absCosTheta(wo);
}

float MicrofacetDistribution::G1(const glm::vec3& w) const
{
    return 1.0f / (1.0f + lambda(w));
}

float MicrofacetDistribution::G(const glm::vec3& wo, const glm::vec3& wi) const
{
    return 1.0f / (1.0f + lambda(wo) + lambda(wi));
}

BeckmannDistribution::BeckmannDistribution(float alphaX, float alphaY)
    : m_alphaX(alphaX)
    , m_alphaY(alphaY)
{
}

float BeckmannDistribution::roughnessToAlpha(float roughness)
{
    roughness = std::max(roughness, 1e-3f);
    float x = std::log(roughness);
    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

// https://github.com/mmp/pbrt-v3/blob/master/src/core/microfacet.cpp
glm::vec3 BeckmannDistribution::sampleWh(const glm::vec3& wo, const glm::vec2& u) const
{
    // Sample visible area of noramls for Beckmann distribution
    glm::vec3 wh;
    bool flip = wo.z < 0.0f;
    wh = beckmannSample(flip ? -wo : wo, m_alphaX, m_alphaY, u[0], u[1]);
    if (flip)
        wh = -wh;
    return wh;
}

float BeckmannDistribution::D(const glm::vec3& wh) const
{
    float tan2Theta = tan2theta(wh);
    if (std::isinf(tan2Theta))
        return 0.0f;

    // clang-format off
    float cos4theta = cos2theta(wh) * cos2theta(wh);
    return std::exp(-tan2Theta * (cos2phi(wh) / (m_alphaX * m_alphaX) +
		                          sin2phi(wh) / (m_alphaY * m_alphaY))) /
		(glm::pi<float>() * m_alphaX * m_alphaY * cos4theta);
    // clang-format on
}

// PBRTv3, page 543
float BeckmannDistribution::lambda(const glm::vec3& w) const
{
    float absTanTheta = std::abs(tanTheta(w));
    if (std::isinf(absTanTheta))
        return 0.0f;

    // Compute alpha for direction w
    // clang-format off
	float alpha = std::sqrt(cos2phi(w) * m_alphaX * m_alphaX +
		                    sin2phi(w) * m_alphaY * m_alphaY);
    // clang-format on
    float a = 1.0f / (alpha * absTanTheta);
    if (a >= 1.6f)
        return 0.0f;

    return (1 - 1.259f * a + 0.396f * a * a) / (3.535f * a + 2.181f * a * a);
}

static glm::vec2 trowbridgeReitzSample11(float cosTheta, float u1, float u2)
{
    // special case (normal incidence)
    if (cosTheta > .9999f) {
        float r = sqrt(u1 / (1 - u1));
        float phi = 6.28318530718f * u2;
        //*slope_x = r * cos(phi);
        //*slope_y = r * sin(phi);
        return glm::vec2(r * cos(phi), r * sin(phi));
    }

	glm::vec2 slope;
    float sinTheta = std::sqrt(std::max((float)0, (float)1 - cosTheta * cosTheta));
    float tanTheta = sinTheta / cosTheta;
    float a = 1 / tanTheta;
    float G1 = 2 / (1 + std::sqrt(1.f + 1.f / (a * a)));

    // sample slope_x
    float A = 2 * u1 / G1 - 1;
    float tmp = 1.f / (A * A - 1.f);
    if (tmp > 1e10f)
        tmp = 1e10f;
    float B = tanTheta;
    float D = std::sqrt(
        std::max(float(B * B * tmp * tmp - (A * A - B * B) * tmp), float(0)));
    float slope_x_1 = B * tmp - D;
    float slope_x_2 = B * tmp + D;
	slope.x = (A < 0 || slope_x_2 > 1.f / tanTheta) ? slope_x_1 : slope_x_2;

    // sample slope_y
    float S;
    if (u2 > 0.5f) {
        S = 1.f;
        u2 = 2.f * (u2 - .5f);
    } else {
        S = -1.f;
        u2 = 2.f * (.5f - u2);
    }
    float z = (u2 * (u2 * (u2 * 0.27385f - 0.73369f) + 0.46341f)) / (u2 * (u2 * (u2 * 0.093073f + 0.309420f) - 1.000000f) + 0.597999f);
	slope.y = S * z * std::sqrt(1.f + slope.x * slope.x);
	return slope;
}

static glm::vec3 trowbridgeReitzSample(const glm::vec3& wi, float alphaX, float alphaY, float u1, float u2)
{
    // 1. stretch wi
    glm::vec3 wiStretched = glm::normalize(glm::vec3(alphaX * wi.x, alphaY * wi.y, wi.z));

    // 2. simulate P22_{wi}(x_slope, y_slope, 1, 1)
    glm::vec2 slope = trowbridgeReitzSample11(cosTheta(wiStretched), u1, u2);

    // 3. rotate
    float tmp = cosPhi(wiStretched) * slope.x - sinPhi(wiStretched) * slope.y;
	slope.y = sinPhi(wiStretched) * slope.x + cosPhi(wiStretched) * slope.y;
	slope.x = tmp;

    // 4. unstretch
	slope.x = alphaX * slope.x;
	slope.y = alphaY * slope.y;

    // 5. compute normal
    return glm::normalize(glm::vec3(-slope.x, -slope.y, 1.));
}

TrowbridgeReitzDistribution::TrowbridgeReitzDistribution(float alphaX, float alphaY)
    : m_alphaX(alphaX)
    , m_alphaY(alphaY)
{
}

float TrowbridgeReitzDistribution::roughnessToAlpha(float roughness)
{
    roughness = std::max(roughness, 1e-3f);
    float x = std::log(roughness);
    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

glm::vec3 TrowbridgeReitzDistribution::sampleWh(const glm::vec3& wo, const glm::vec2& u) const
{
	bool flip = wo.z < 0;
	glm::vec3 wh = trowbridgeReitzSample(flip ? -wo : wo, m_alphaX, m_alphaY, u[0], u[1]);
	if (flip)
		wh = -wh;
	return wh;
}

float TrowbridgeReitzDistribution::D(const glm::vec3& wh) const
{
    float tan2Theta = tan2theta(wh);
    if (std::isinf(tan2Theta))
        return 0.0f;

    // clang-format off
	float cos4theta = cos2theta(wh) * cos2theta(wh);
	float e = (cos2phi(wh) / (m_alphaX * m_alphaX) +
		sin2phi(wh) / (m_alphaY * m_alphaY)) * tan2Theta;
	return 1.0f / (glm::pi<float>() * m_alphaX * m_alphaY * cos4theta * (1.0f + e) * (1.0f + e));
    // clang-format on
}

float TrowbridgeReitzDistribution::lambda(const glm::vec3& w) const
{
    float absTanTheta = std::abs(tanTheta(w));
    if (std::isinf(absTanTheta))
        return 0.0f;

    // Compute alpha for direction w
    // clang-format off
	float alpha = std::sqrt(cos2phi(w) * m_alphaX * m_alphaX +
		sin2phi(w) * m_alphaY * m_alphaY);
    // clang-format on

    float alpha2tan2theta = (alpha * absTanTheta) * (alpha * absTanTheta);
    return (-1.0f + std::sqrt(1.0f + alpha2tan2theta)) / 2.0f;
}

}
