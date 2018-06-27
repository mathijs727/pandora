#include "reflection/fresnel_blend_bxdf.h"
#include "fresnel_blend_bxdf.h"
#include "pandora/utility/math.h"
#include "reflection/fresnel.h"
#include "reflection/helpers.h"
#include "reflection/microfacet.h"
#include <glm/gtc/constants.hpp>

namespace pandora {

FresnelBlendBxDF::FresnelBlendBxDF(const Spectrum& rd, const Spectrum& rs, const MicrofacetDistribution& distribution)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_GLOSSY))
    , m_rd(rd)
    , m_rs(rs)
    , m_distribution(distribution)
{
}

Spectrum FresnelBlendBxDF::schlickFresnel(float cosTheta) const
{
    auto pow5 = [](float v) { return (v * v) * (v * v) * v; };
    return m_rs + pow5(1.0f - cosTheta) * (Spectrum(1.0f) - m_rs);
}

// PBRTv3 page 551
Spectrum FresnelBlendBxDF::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    auto pow5 = [](float v) { return (v * v) * (v * v) * v; };
    // clang-format off
	Spectrum diffuse = (28.0f / (23.0f * glm::pi<float>())) * m_rd *
		(Spectrum(1.0f) - m_rs) *
		(1.0f - pow5(1.0f - 0.5f * absCosTheta(wi))) *
		(1.0f - pow5(1.0f - 0.5f * absCosTheta(wo)));
    // clang-format on

    glm::vec3 wh = wi + wo;
    if (wh.x == 0.0f && wh.y == 0.0f && wh.z == 0.0f)
        return Spectrum(0.0f);
    wh = glm::normalize(wh);

    Spectrum specular = m_distribution.D(wh) / (4 * absDot(wi, wh) * std::max(absCosTheta(wi), absCosTheta(wo))) * schlickFresnel(glm::dot(wi, wh));
	return diffuse = specular;
}

}
