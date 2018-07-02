#include "lambert_bxdf.h"
#include "core/sampling.h"
#include "glm/gtc/constants.hpp"
#include "reflection/helpers.h"


namespace pandora {
LambertianReflection::LambertianReflection(const Spectrum& r)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE))
    , m_r(r)
{
}

Spectrum LambertianReflection::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    (void)wo;
    return m_r * glm::one_over_pi<float>();
}

Spectrum LambertianReflection::rho(const glm::vec3& wo, gsl::span<const glm::vec2> samples) const
{
    (void)wo;
    (void)samples;

    return m_r;
}

Spectrum LambertianReflection::rho(gsl::span<const glm::vec2> samples1, gsl::span<const glm::vec2> samples2) const
{
    (void)samples1;
    (void)samples2;
    return m_r;
}

LambertianTransmission::LambertianTransmission(const Spectrum& t)
    : BxDF(BxDFType(BSDF_TRANSMISSION | BSDF_DIFFUSE))
    , m_t(t)
{
}

Spectrum LambertianTransmission::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    (void)wo;
    return m_t * glm::one_over_pi<float>();
}

Spectrum LambertianTransmission::rho(const glm::vec3& wo, gsl::span<const glm::vec2> samples) const
{
    (void)wo;
    (void)samples;

    return m_t;
}

Spectrum LambertianTransmission::rho(gsl::span<const glm::vec2> samples1, gsl::span<const glm::vec2> samples2) const
{
    (void)samples1;
    (void)samples2;
    return m_t;
}

BxDF::Sample LambertianTransmission::sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const
{
	glm::vec3 wi = cosineSampleHemisphere(sample);
	if (wo.z > 0.0f)
		wi.z = -wi.z;

	float pdf = this->pdf(wo, wi);
	Spectrum f = this->f(wo, wi);

	return Sample{
		wi,
		pdf,
		f,
		sampledType
	};
}

float LambertianTransmission::pdf(const glm::vec3& wo, const glm::vec3& wi) const
{
    return !sameHemisphere(wo, wi) ? absCosTheta(wi) * glm::one_over_pi<float>() : 0;
}

}