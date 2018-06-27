#include "reflection/microfacet_bxdf.h"
#include "pandora/utility/math.h"
#include "reflection/fresnel.h"
#include "reflection/helpers.h"
#include <iostream>

namespace pandora {

MicrofacetReflection::MicrofacetReflection(const Spectrum& r, const MicrofacetDistribution& distribution, const Fresnel& fresnel)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_GLOSSY))
    , m_r(r)
    , m_distribution(distribution)
    , m_fresnel(fresnel)
{
}

// PBRTv3 page 547
Spectrum MicrofacetReflection::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    float cosThetaO = absCosTheta(wo), cosThetaI = absCosTheta(wi);
    glm::vec3 wh = wi + wo; // Half vector

    // Handle degenerate cases for microfacet reflection
    if (cosThetaI == 0.0f || cosThetaO == 0.0f)
        return Spectrum(0.0f);
    if (wh.x == 0.0f && wh.y == 0.0f && wh.z == 0.0f)
        return Spectrum(0.0f);

    wh = glm::normalize(wh);
    Spectrum F = m_fresnel.evaluate(glm::dot(wi, wh));
    return m_r * m_distribution.D(wh) * m_distribution.G(wo, wi) * F / (4 * cosThetaI * cosThetaO);
}

// PBRTv3 page 811
BxDF::Sample MicrofacetReflection::sampleF(const glm::vec3& wo, const glm::vec2& u, BxDFType sampledType) const
{
    // Sample microfacet orientation wh and reflected direction wi
    glm::vec3 wh = m_distribution.sampleWh(wo, u);
    glm::vec3 wi = reflect(wo, wh);
    if (!sameHemisphere(wo, wi)) {
        Sample result;
        result.f = Spectrum(0.0f);
        result.pdf = 0.0f;
        return result;
    }

    // Compute PDF of wi for microfacet reflection
    float pdf = m_distribution.pdf(wo, wh) / (4 * glm::dot(wo, wh));

    Sample result;
    result.wi = wi;
    result.pdf = pdf;
    result.f = f(wo, result.wi);
    result.sampledType = sampledType;
    return result;
}

// PBRTv3 page 813
float MicrofacetReflection::pdf(const glm::vec3& wo, const glm::vec3& wi) const
{
    if (!sameHemisphere(wo, wi))
        return 0.0f;
    glm::vec3 wh = glm::normalize(wo + wi);
    return m_distribution.pdf(wo, wh) / (4 * glm::dot(wo, wh));
}

MicrofacetTransmission::MicrofacetTransmission(const Spectrum& t, const MicrofacetDistribution& distribution, float etaA, float etaB, TransportMode mode)
    : BxDF(BxDFType(BSDF_TRANSMISSION | BSDF_GLOSSY))
    , m_t(t)
    , m_distribution(distribution)
    , m_etaA(etaA)
    , m_etaB(etaB)
    , m_fresnel(etaA, etaB)
    , m_mode(mode)

{
}

// https://github.com/mmp/pbrt-v3/blob/master/src/core/reflection.cpp
Spectrum MicrofacetTransmission::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    if (sameHemisphere(wo, wi))
        return Spectrum(0.0f); // Transmission only

    float cosThetaO = cosTheta(wo);
    float cosThetaI = cosTheta(wi);
    if (cosThetaI = 0.0f || cosThetaO == 0.0f)
        return Spectrum(0.0f);

    // Compute wh from wo and wi from microfacet distribution
    float eta = cosTheta(wo) > 0.0 ? (m_etaB / m_etaA) : (m_etaA / m_etaB);
    glm::vec3 wh = glm::normalize(wo + wi * eta);
    if (wh.z < 0.0f)
        wh = -wh;

    Spectrum f = m_fresnel.evaluate(glm::dot(wo, wh));

    float sqrtDenom = glm::dot(wo, wh) + eta * glm::dot(wi, wh);
    float factor = (m_mode == TransportMode::Radiance) ? (1.0f / eta) : 1.0f;

    // clang-format off
	return (Spectrum(1.f) - f) * m_t *
		std::abs(m_distribution.D(wh) * m_distribution.G(wo, wi) * eta * eta *
			absDot(wi, wh) * absDot(wo, wh) * factor * factor /
			(cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));
    // clang-format on
}

// PBRTv3 page 813
BxDF::Sample MicrofacetTransmission::sampleF(const glm::vec3& wo, const glm::vec2& u, BxDFType sampledType) const
{
    glm::vec3 wh = m_distribution.sampleWh(wo, u);
    float eta = cosTheta(wo) > 0 ? (m_etaA / m_etaB) : (m_etaB / m_etaA);
    if (auto wiOpt = refract(wo, wh, eta); wiOpt) {
		Sample sample;
		sample.wi = *wiOpt;
		sample.pdf = pdf(wo, sample.wi);
		sample.f = f(wo, sample.wi);
		sample.sampledType = sampledType;
		return sample;
    } else {
		Sample sample;
		sample.pdf = 0.0f;
		sample.f = Spectrum(0.0f);
		return sample;
    }
}

// PBRTv3 page 814
float MicrofacetTransmission::pdf(const glm::vec3& wo, const glm::vec3& wi) const
{
	if (sameHemisphere(wo, wi))
		return 0.0f;

	// Compute wh from wo and wi for microfacet transmission
	float eta = cosTheta(wo) > 0.0f ? (m_etaB / m_etaA) : (m_etaA / m_etaB);
	glm::vec3 wh = glm::normalize(wo + wi * eta);

	// Compute change of variables dwh_dwi for microfacet transmission
	// https://github.com/mmp/pbrt-v3/blob/master/src/core/reflection.cpp
	float sqrtDenom = glm::dot(wo, wh) + eta * glm::dot(wi, wh);
	float dwhDwi = std::abs((eta * eta * glm::dot(wi, wh)) / (sqrtDenom * sqrtDenom));

	return m_distribution.pdf(wo, wh) * dwhDwi;
}

}
