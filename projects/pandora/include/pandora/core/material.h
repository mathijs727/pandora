#pragma once
#include "EASTL/fixed_vector.h"
#include "bxdf.h"
#include "gsl/gsl"
#include "pandora/core/pandora.h"

namespace pandora {

class BSDF {
public:
    BSDF(const SurfaceInteraction& si, float eta = 1);
    ~BSDF() = delete;// Should never be called if BSDF is allocated (and freed) through a memory arena

    void add(const BxDF* b);
    int numComponents(BxDFType flags = BSDF_ALL) const;

    glm::vec3 worldToLocal(const glm::vec3& v) const;
    glm::vec3 localToWorld(const glm::vec3& v) const;

    // wo and wi in world coordinates
    Spectrum f(const glm::vec3& woW, const glm::vec3& wiW, BxDFType flags = BSDF_ALL) const;

    // Total reflection in a given direction due to constant illumination over the hemisphere
    Spectrum rho(const glm::vec3& wo, gsl::span<const glm::vec2> samples, BxDFType flags = BSDF_ALL) const;

    // Fraction of incident light reflected by a surface when the incident light is the same from all directions
    Spectrum rho(gsl::span<const glm::vec2> samples1, gsl::span<const glm::vec2> samples2, BxDFType flags = BSDF_ALL) const;
private:
    const float m_eta;
    const glm::vec3 m_ns, m_ng; // Shading & geometric normal
    const glm::vec3 m_ss, m_ts; // Shading tangent & bitangent

    static constexpr int MAX_BxDFs = 8;
    eastl::fixed_vector<const BxDF*, MAX_BxDFs> m_bxdfs;
};

class Material {
public:
    struct EvalResult {
        glm::vec3 weigth;
        float pdf;
    };
    virtual EvalResult evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const = 0;

    struct SampleResult {
        glm::vec3 multiplier;
        float pdf;
        glm::vec3 out;
    };
    virtual SampleResult sampleBSDF(const SurfaceInteraction& surfaceInteraction, gsl::span<glm::vec2> samples) const = 0;
};

}
