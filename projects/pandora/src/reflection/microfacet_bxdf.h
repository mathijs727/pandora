#pragma once
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/pandora.h"
#include "reflection/fresnel.h"
#include "reflection/microfacet.h"

namespace pandora {

class MicrofacetReflection : public BxDF {
public:
    MicrofacetReflection(const Spectrum& r, const MicrofacetDistribution& distribution, const Fresnel& fresnel);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

	Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType = BSDF_ALL) const override final;
	float pdf(const glm::vec3& wo, const glm::vec3& wi) const override final;
private:
    const Spectrum m_r;
    const MicrofacetDistribution& m_distribution;
    const Fresnel& m_fresnel;
};

class MicrofacetTransmission : public BxDF {
public:
    MicrofacetTransmission(const Spectrum& t, const MicrofacetDistribution& distribution, float etaA, float etaB);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

	Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType = BSDF_ALL) const override final;
	float pdf(const glm::vec3& wo, const glm::vec3& wi) const override final;
private:
    const Spectrum m_t;
    const MicrofacetDistribution& m_distribution;
    const float m_etaA, m_etaB;
    const FresnelDielectric m_fresnel;
};

}
