#pragma once
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/texture.h"

namespace pandora {

class MatteMaterial : public Material {
public:
    // kd = diffuse reflection, sigma = roughness, 
    MatteMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<float>>& sigma);

    void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;
private:
    std::shared_ptr<Texture<Spectrum>> m_kd;// Diffuse reflection
    std::shared_ptr<Texture<float>> m_sigma;// Roughness
};

}
