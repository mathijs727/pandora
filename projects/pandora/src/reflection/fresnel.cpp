#include "fresnel.h"
#include <algorithm>

namespace pandora
{

float frDielectric(float cosThetaI, float etaI, float etaT)
{
    // PBRTv3 page 516
    cosThetaI = std::clamp(cosThetaI, -1.0f, 1.0f);

    // Potentially swap indices of refraction
    bool entering = cosThetaI > 0.0f;
    if (!entering) {
        std::swap(etaI, etaT);
        cosThetaI = std::abs(cosThetaI);
    }

    // Compute cosThetatT using Snell's law
    float sinThetaI = std::sqrt(std::max(0.0f, 1.0f - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;
    if (sinThetaT >= 1.0f) // Total internal reflection
        return 1.0f;
    float cosThetaT = std::sqrt(std::max(0.0f, 1.0f - sinThetaT * sinThetaT));

    float rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
    float rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (rparl * rparl + rperp * rperp) / 2.0f;
}

glm::vec3 frConductor(float cosThetaI, const glm::vec3& etaI, const glm::vec3& etaT, const glm::vec3& k)
{
    cosThetaI = std::clamp(cosThetaI, -1.0f, 1.0f);
    Spectrum eta = etaT / etaI;
    Spectrum etaK = k / etaI;

    float cosThetaI2 = cosThetaI * cosThetaI;
    float sinThetaI2 = 1.0f - cosThetaI2;
    Spectrum eta2 = eta * eta;
    Spectrum etaK2 = etaK * etaK;

    Spectrum t0 = eta2 - etaK2 - sinThetaI2;
    Spectrum a2plusb2 = glm::sqrt(t0 * t0 + 4.0f * eta2 * etaK2);
    Spectrum t1 = a2plusb2 + cosThetaI2;
    Spectrum a = glm::sqrt(0.5f * (a2plusb2 + t0));
    Spectrum t2 = 2.0f * cosThetaI * a;
    Spectrum rs = (t1 - t2) / (t1 + t2);

    Spectrum t3 = cosThetaI2 * a2plusb2 + sinThetaI2 * sinThetaI2;
    Spectrum t4 = t2 * sinThetaI2;
    Spectrum rp = rs * (t3 - t4) / (t3 + t4);

    return 0.5f * (rp + rs);
}

FresnelDielectric::FresnelDielectric(float etaI, float etaT)
    : m_etaI(etaI)
    , m_etaT(etaT)
{
}

Spectrum FresnelDielectric::evaluate(float cosThetaI) const
{
    return Spectrum(frDielectric(cosThetaI, m_etaI, m_etaT));
}

FresnelConductor::FresnelConductor(const Spectrum& etaI, const Spectrum& etaT, const Spectrum k)
    : m_etaI(etaI)
    , m_etaT(etaT)
    , m_k(k)
{
}

Spectrum FresnelConductor::evaluate(float cosThetaI) const
{
    return frConductor(std::abs(cosThetaI), m_etaI, m_etaT, m_k);
}

Spectrum FresnelNoOp::evaluate(float cosThetaI) const
{
    return Spectrum(1.0f);
}

}
