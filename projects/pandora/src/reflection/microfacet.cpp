#include "reflection/microfacet.h"
#include "glm/gtc/constants.hpp"
#include "reflection/helpers.h"
#include <algorithm>

namespace pandora {

float MicrofacetDistribution::G1(const glm::vec3& w) const
{
    return 1.0f / (1.0f + lambda(w));
}

float MicrofacetDistribution::G(const glm::vec3 & wo, const glm::vec3 & wi) const
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
