#include "scaled_bxdf.h"

namespace pandora {

ScaledBxDF::ScaledBxDF(BxDF& bxdf, const Spectrum& scale)
    : BxDF(bxdf.getType())
    , m_bxdf(bxdf)
    , m_scale(scale)
{
}

Spectrum ScaledBxDF::f(const glm::vec3& wo, const glm::vec3& wi) const
{
    return m_scale * m_bxdf.f(wo, wi);
}

}
