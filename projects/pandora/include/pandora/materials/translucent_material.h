#pragma once
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/texture.h"

namespace pandora {

	class TranslucentMaterial : public Material {
	public:
		// kd = diffuse reflection, sigma = roughness, 
		TranslucentMaterial(
            const std::shared_ptr<Texture<Spectrum>>& kd,
            const std::shared_ptr<Texture<Spectrum>>& ks,
            const std::shared_ptr<Texture<float>>& roughness,
            const std::shared_ptr<Texture<Spectrum>>& reflect,
            const std::shared_ptr<Texture<Spectrum>>& transmit,
            bool remapRoughness);

		void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;
	private:
		std::shared_ptr<Texture<Spectrum>> m_kd, m_ks;// Diffuse / specular
        std::shared_ptr<Texture<float>> m_roughness;
		std::shared_ptr<Texture<Spectrum>> m_reflect, m_transmit;
        bool m_remapRoughness;
	};

}
