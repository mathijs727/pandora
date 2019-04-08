#pragma once
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/texture.h"

namespace pandora {

class PlasticMaterial : public Material {
public:
    // If remapRoughness = true then roughness is expected to be on a scale from 0.0f to 1.0f. Otherwise it is used directly to inialize the microfacet distributions
	PlasticMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<Spectrum>>& ks, const std::shared_ptr<Texture<float>>& roughness, bool remapRoughness = true);

    void computeScatteringFunctions(SurfaceInteraction& si, ShadingMemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;

private:
    const std::shared_ptr<Texture<Spectrum>> m_kd, m_ks; // Diffuse & specular reflection
    const std::shared_ptr<Texture<float>> m_roughness;
	const bool m_remapRoughness;
};

}
