#pragma once
#include "pandora/core/material.h"
#include "pandora/core/texture.h"

namespace pandora {

class MatteMaterial : public Material {
public:
    // kd = diffuse reflection, sigma = roughness, 
    MatteMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<float>>& sigma);

    //EvalResult evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const final;
    //SampleResult sampleBSDF(const SurfaceInteraction& surfaceInteraction, gsl::span<glm::vec2> samples) const final;

    void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;
private:
    std::shared_ptr<Texture<Spectrum>> m_kd;// Diffuse reflection
    std::shared_ptr<Texture<float>> m_sigma;// Roughness
};

}
