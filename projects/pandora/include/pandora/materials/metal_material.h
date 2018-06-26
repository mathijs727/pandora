#pragma once
#include "pandora/core/material.h"
#include "pandora/core/texture.h"

namespace pandora {

class MetalMaterial : public Material {
public:
    // kd = diffuse reflection, sigma = roughness,
    MetalMaterial(
        const std::shared_ptr<Texture<Spectrum>>& eta,
        const std::shared_ptr<Texture<Spectrum>>& k,
        const std::shared_ptr<Texture<float>>& urough,
        const std::shared_ptr<Texture<float>>& vrough,
        bool remapRoughness = true);
	MetalMaterial(
		const std::shared_ptr<Texture<Spectrum>>& eta,
		const std::shared_ptr<Texture<Spectrum>>& k,
		const std::shared_ptr<Texture<float>>& rough,
		bool remapRoughness = true);
    static std::shared_ptr<MetalMaterial> createCopper(const std::shared_ptr<Texture<float>>& roughness, bool remapRoughness);
    static std::shared_ptr<MetalMaterial> createCopper(const std::shared_ptr<Texture<float>>& uRoughness, const std::shared_ptr<Texture<float>>& vRoughness, bool remapRoughness);

    void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;

private:
    const std::shared_ptr<Texture<Spectrum>> m_eta, m_k; // Diffuse reflection
    const std::shared_ptr<Texture<float>> m_roughness, m_uRoughness, m_vRoughness; // Roughness
    const bool m_remapRoughness;
};

}
