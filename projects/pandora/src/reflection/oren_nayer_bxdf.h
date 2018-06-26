#pragma once
#include "pandora/core/bxdf.h"
#include "pandora/core/pandora.h"

namespace pandora {

class OrenNayerBxDF : public BxDF {
public:
	OrenNayerBxDF(const Spectrum& r, float sigma);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

private:
    const Spectrum m_r;
	float m_a, m_b;
};

}
