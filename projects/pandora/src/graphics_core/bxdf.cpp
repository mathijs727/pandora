#include "pandora/graphics_core/bxdf.h"
#include "core/sampling.h"
#include "pandora/utility/math.h"
#include "reflection/helpers.h"
#include <glm/gtc/constants.hpp>

namespace pandora {
BxDF::BxDF(BxDFType type)
    : m_type(type)
{
}

BxDF::Sample BxDF::sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType) const
{
    // Cosine-sample the hemisphere, flipping the direction of necessary
    glm::vec3 wi = cosineSampleHemisphere(sample);
    if (wo.z < 0.0f)
        wi.z = -wi.z;

    float pdf = this->pdf(wo, wi);
    Spectrum f = this->f(wo, wi);

    return Sample {
        wi,
        pdf,
        f,
        sampledType
    };
}

Spectrum BxDF::rho(const glm::vec3 & wo, gsl::span<const glm::vec2> samples) const
{
    Spectrum r(0.0f);
    for (const glm::vec2& u : samples) {
        auto sample = sampleF(wo, u);
        if (sample.pdf > 0.0f)
            r += sample.f * absCosTheta(sample.wi) / sample.pdf;
    }
    return r / (float)samples.size();
}

Spectrum BxDF::rho(gsl::span<const glm::vec2> u1, gsl::span<const glm::vec2> u2) const
{
    assert(u1.size() == u2.size());

    Spectrum r(0.0f);
    for (int i = 0; i < u1.size(); i++) {
        glm::vec3 wo, wi;
        wo = uniformSampleHemisphere(u1[i]);
        float pdfo = uniformHemispherePdf();
        auto sample = sampleF(wo, u2[i]);
        float pdfi = sample.pdf;
        if (pdfi > 0.0f)
            r += sample.f * absCosTheta(wi) * absCosTheta(wo) / (pdfo * pdfi);
    }
    return r / (glm::pi<float>() * u1.size());
}

float BxDF::pdf(const glm::vec3& wo, const glm::vec3& wi) const
{
    // Cosine weighted distribution. Works well for diffuse BxDFs
    return sameHemisphere(wo, wi) ? absCosTheta(wi) * glm::one_over_pi<float>() : 0.0f;
}

BxDFType BxDF::getType() const
{
    return m_type;
}

bool BxDF::matchesFlags(BxDFType t) const
{
    return (m_type & t) == m_type;
}

}
