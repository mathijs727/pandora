#include "reflection/oren_nayer_bxdf.h"
#include "reflection/helpers.h"
#include <glm/gtc/constants.hpp>

namespace pandora {

// PBRTv3 page 536
OrenNayerBxDF::OrenNayerBxDF(const Spectrum& r, float sigma)
    : BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE))
    , m_r(r)
{
    sigma = glm::radians(sigma);
    float sigma2 = sigma * sigma;
    m_a = 1.0f - (sigma2 / (2.0f * (sigma2 + 0.33f)));
    m_b = 0.45f * sigma2 / (sigma2 + 0.09f);
}

// PBRTv3 page 536
Spectrum OrenNayerBxDF::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    float sinThetaI = sinTheta(wi);
    float sinThetaO = sinTheta(wo);

    // Compute cosine term of Oren-Nayer model
    float maxCos = 0.0f;
    if (sinThetaI > 1e-4 && sinThetaO > 1e-4) {
        float sinPhiI = sinPhi(wi), cosPhiI = cosPhi(wi);
        float sinPhiO = sinPhi(wo), cosPhiO = cosPhi(wo);
        float dCos = cosPhiI * cosPhiO + sinPhiI * sinPhiO;
        maxCos = std::max(0.0f, dCos);
    }

    // Compute sine and tangent terms of Oren-Nayer model
    float sinAlpha, tanBeta;
    if (absCosTheta(wi) > absCosTheta(wo)) {
        sinAlpha = sinThetaO;
        tanBeta = sinThetaI / absCosTheta(wi);
    } else {
        sinAlpha = sinThetaI;
        tanBeta = sinThetaO / absCosTheta(wo);
    }

    return m_r * glm::one_over_pi<float>() * (m_a + m_b * maxCos * sinAlpha * tanBeta);
}

}
