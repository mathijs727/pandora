#pragma once
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/pandora.h"

namespace pandora {

class LambertianReflection : public BxDF {
public:
    LambertianReflection(const Spectrum& r);

    Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

    Spectrum rho(const glm::vec3& wo, std::span<const glm::vec2> samples) const final;
    Spectrum rho(std::span<const glm::vec2> samples1, std::span<const glm::vec2> samples2) const final;

private:
    const Spectrum m_r;
};


class LambertianTransmission : public BxDF {
public:
	LambertianTransmission(const Spectrum& t);

	Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const final;

	Spectrum rho(const glm::vec3& wo, std::span<const glm::vec2> samples) const final;
	Spectrum rho(std::span<const glm::vec2> samples1, std::span<const glm::vec2> samples2) const final;

	Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType = BSDF_ALL) const final;
	float pdf(const glm::vec3& wo, const glm::vec3& wi) const final;
private:
	const Spectrum m_t;
};
}
