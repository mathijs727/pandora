#pragma once
#include "pandora/core/material.h"
#include "pandora/core/texture.h"

namespace pandora {

	class TranslucentMaterial : public Material {
	public:
		// kd = diffuse reflection, sigma = roughness, 
		TranslucentMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<float>>& reflect, const std::shared_ptr<Texture<float>>& transmit);

		void computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const final;
	private:
		std::shared_ptr<Texture<Spectrum>> m_kd;// Diffuse / specular
		std::shared_ptr<Texture<Spectrum>> m_reflect, m_transmit;
	};

}
