#pragma once
#include "fresnel.h"
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/pandora.h"

namespace pandora {
class SpecularReflection : public BxDF {
public:
    SpecularReflection(const Spectrum& r, Fresnel& fresnel);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;
    Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const final;

private:
    const Spectrum m_r;
    const Fresnel& m_fresnel;
};

class SpecularTransmission : public BxDF {
public:
    SpecularTransmission(const Spectrum& t, float etaA, float etaB, TransportMode mode);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;
    Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const final;

private:
    const Spectrum m_t;
    const float m_etaA, m_etaB;
    const FresnelDielectric m_fresnel;
    const TransportMode m_mode;
};

class FresnelSpecular : public BxDF {
public:
    FresnelSpecular(const Spectrum& r, const Spectrum& t, float etaA, float etaB, TransportMode transportMode);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;
    Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const final;

private:
    const Spectrum m_r, m_t;
    const float m_etaA, m_etaB;
    const FresnelDielectric m_fresnel;
    const TransportMode m_mode;
};

}
