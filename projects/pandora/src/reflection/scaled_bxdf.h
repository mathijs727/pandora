#pragma once
#include "pandora/core/bxdf.h"

namespace pandora {
class ScaledBxDF : public BxDF {
public:
    ScaledBxDF(BxDF& bxdf, const glm::vec3& scale);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

private:
    BxDF& m_bxdf;
    Spectrum m_scale;
};

}
