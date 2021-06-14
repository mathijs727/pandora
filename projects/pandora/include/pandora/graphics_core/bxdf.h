#pragma once
#include "pandora/graphics_core/pandora.h"
#include <span>

namespace pandora {

// PBRTv3 page 513
enum BxDFType {
    BSDF_REFLECTION = 1 << 0,
    BSDF_TRANSMISSION = 1 << 1,
    BSDF_DIFFUSE = 1 << 2,
    BSDF_GLOSSY = 1 << 3,
    BSDF_SPECULAR = 1 << 4,
    BSDF_ALL = BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR | BSDF_REFLECTION | BSDF_TRANSMISSION
};

class BxDF {
public:
    BxDF(BxDFType type);

    struct Sample {
        glm::vec3 wi;
        float pdf;
        Spectrum f;
        BxDFType sampledType;
    };
    virtual Spectrum f(const glm::vec3& wo, const glm::vec3& wi) const = 0;

    // Total reflection in a given direction due to constant illumination over the hemisphere
    virtual Spectrum rho(const glm::vec3& wo, std::span<const glm::vec2> samples) const;

    // Fraction of incident light reflected by a surface when the incident light is the same from all directions
    virtual Spectrum rho(std::span<const glm::vec2> samples1, std::span<const glm::vec2> samples2) const;

    virtual Sample sampleF(const glm::vec3& wo, const glm::vec2& sample, BxDFType sampledType = BSDF_ALL) const;
    virtual float pdf(const glm::vec3& wo, const glm::vec3& wi) const;

    BxDFType getType() const;
    bool matchesFlags(BxDFType t) const;

private:
    BxDFType m_type;
};

}
