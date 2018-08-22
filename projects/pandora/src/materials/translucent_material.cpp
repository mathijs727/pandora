#include "pandora/materials/translucent_material.h"
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/lambert_bxdf.h"
#include "shading.h"

namespace pandora {
TranslucentMaterial::TranslucentMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<Spectrum>>& reflect, const std::shared_ptr<Texture<Spectrum>>& transmit) :
	m_kd(kd), m_reflect(reflect), m_transmit(transmit)
{
}

void TranslucentMaterial::computeScatteringFunctions(SurfaceInteraction& si, ShadingMemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const
{
	// TODO: perform bump mapping (normal mapping)

	// Evaluate textures and allocate BRDF
	si.bsdf = arena.allocate<BSDF>(si);

	Spectrum r = glm::clamp(m_reflect->evaluate(si), 0.0f, 1.0f);
	Spectrum t = glm::clamp(m_transmit->evaluate(si), 0.0f, 1.0f);
	if (isBlack(r) || isBlack(t))
		return;

	Spectrum kd = glm::clamp(m_kd->evaluate(si), 0.0f, 1.0f);
	if (!isBlack(kd)) {
		if (!isBlack(r))
			si.bsdf->add(arena.allocate<LambertianReflection>(r * kd));
		if (!isBlack(t))
			si.bsdf->add(arena.allocate<LambertianTransmission>(t * kd));
	}

	// TODO: specular (https://github.com/mmp/pbrt-v3/blob/master/src/materials/translucent.cpp)
}
}
