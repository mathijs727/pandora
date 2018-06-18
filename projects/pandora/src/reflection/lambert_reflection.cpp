#include "lambert_reflection.h"
#include "glm/gtc/constants.hpp"

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
}
