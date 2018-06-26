#pragma once
#include "pandora/core/pandora.h"

namespace pandora {

// PBRTv3 page 537
class MicrofacetDistribution {
public:
    // Differential area of microfacets oriented with the given normal
    virtual float D(const glm::vec3& wh) const = 0;

    // Measures invisible masked mirofacet area per visible microfacet area
    virtual float lambda(const glm::vec3& w) const = 0;

    // Masking function (based on lambda)
    float G1(const glm::vec3& w) const;

	// Fraction of microfacets in a differential area that are visible from both directions
	float G(const glm::vec3& wo, const glm::vec3& wi) const;
protected:
};

class BeckmannDistribution : public MicrofacetDistribution {
public:
    BeckmannDistribution(float alphaX, float alphaY);
    static float roughnessToAlpha(float roughness);

    float D(const glm::vec3& wh) const override final;
    float lambda(const glm::vec3& w) const override final;

private:
    const float m_alphaX, m_alphaY;
};

class TrowbridgeReitzDistribution : public MicrofacetDistribution {
public:
    TrowbridgeReitzDistribution(float alphaX, float alphaY);
    static float roughnessToAlpha(float roughness);

    float D(const glm::vec3& wh) const override final;
    float lambda(const glm::vec3& w) const override final;

private:
    const float m_alphaX, m_alphaY;
};

}
