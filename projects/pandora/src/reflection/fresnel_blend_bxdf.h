#pragma once
#include "pandora/core/bxdf.h"
#include "pandora/core/pandora.h"
#include "reflection/fresnel.h"

namespace pandora {

class MicrofacetDistribution;

class FresnelBlendBxDF : public BxDF {
public:
	FresnelBlendBxDF(const Spectrum& rd, const Spectrum& rs, const MicrofacetDistribution& distribution);
	Spectrum schlickFresnel(float cosTheta) const;

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;
private:
    const Spectrum m_rd, m_rs;
    const MicrofacetDistribution& m_distribution;
};

}
