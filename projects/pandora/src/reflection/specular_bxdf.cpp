#include "specular_bxdf.h"
#include "helpers.h"
#include "pandora/utility/math.h"

namespace pandora {
SpecularReflection::SpecularReflection(const Spectrum& r, Fresnel& fresnel)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_SPECULAR))
    , m_r(r)
    , m_fresnel(fresnel)
{
}

Spectrum SpecularReflection::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    // No scattering since for an arbitrary pair of directions the delta function returns no scatering
    return Spectrum(0.f);
}

BxDF::Sample SpecularReflection::sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const
{
    // Compute perfect specular reflection direction
    glm::vec3 wi = glm::vec3(-wo.x, -wo.y, wo.z);

    BxDF::Sample result;
    result.pdf = 1.0f;
    result.wi = wi;
    result.multiplier = m_fresnel.evaluate(cosTheta(wi)) * m_r / absCosTheta(wi);
    return result;
}

SpecularTransmission::SpecularTransmission(const Spectrum& t, float etaA, float etaB, TransportMode mode)
    : BxDF(BxDFType(BSDF_TRANSMISSION | BSDF_SPECULAR))
    , m_t(t)
    , m_etaA(etaA)
    , m_etaB(etaB)
    , m_fresnel(etaA, etaB)
    , m_mode(mode)
{
}

Spectrum SpecularTransmission::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    return Spectrum(0.0f);
}

BxDF::Sample SpecularTransmission::sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const
{
    // Figure out which refraction index is incident and which is transmitted
    bool entering = cosTheta(wo) > 0.0f;
    float etaI = entering ? m_etaA : m_etaB;
    float etaT = entering ? m_etaB : m_etaA;

    // Compute ray direction for specular transmission
    if (auto wiOpt = refract(wo, faceForward(glm::vec3(0, 0, 1), wo), etaI / etaT); wiOpt) {
        glm::vec3 wi = *wiOpt;

        Spectrum ft = m_t * (Spectrum(1.0f) - m_fresnel.evaluate(cosTheta(wi)));
        BxDF::Sample result;
        result.pdf = 1;
        result.wi = wi;
        result.multiplier = ft / absCosTheta(wi);
        return result;
    } else {
        BxDF::Sample result;
        result.multiplier = glm::vec3(0.0f);
        return result;
    }
}

FresnelSpecular::FresnelSpecular(const Spectrum& r, const Spectrum& t, float etaA, float etaB, TransportMode transportMode)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_SPECULAR))
    , m_r(r)
    , m_t(t)
    , m_etaA(etaA)
    , m_etaB(etaB)
    , m_fresnel(etaA, etaB)
    , m_mode(transportMode)
{
}
Spectrum FresnelSpecular::f(const glm::vec3 & wo, const glm::vec3 & wi) const
{
    return Spectrum(0.0f);
}

BxDF::Sample FresnelSpecular::sampleF(const glm::vec3 & wo, const glm::vec2 & sample, BxDFType sampledType) const
{
    // TODO: section 14.1.3 of PBRTv3
    return Sample();
}
}
