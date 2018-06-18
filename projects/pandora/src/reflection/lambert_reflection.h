#pragma once
#include "pandora/core/bxdf.h"
#include "pandora/core/pandora.h"

namespace pandora {

class LambertianReflection : public BxDF {
public:
    LambertianReflection(const Spectrum& r);

     Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

     Spectrum rho(const glm::vec3& wo, gsl::span<const glm::vec2> samples) const final;
     Spectrum rho(gsl::span<const glm::vec2> samples1, gsl::span<const glm::vec2> samples2) const final;
private:
    const Spectrum m_r;
};

}
