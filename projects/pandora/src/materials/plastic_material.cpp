#include "pandora/materials/plastic_material.h"
#include "glm/glm.hpp"
#include "pandora/graphics_core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/fresnel.h"
#include "reflection/lambert_bxdf.h"
#include "reflection/microfacet.h"
#include "reflection/microfacet_bxdf.h"
#include "reflection/oren_nayer_bxdf.h"
#include "shading.h"

namespace pandora {

PlasticMaterial::PlasticMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<Spectrum>>& ks, const std::shared_ptr<Texture<float>>& roughness, bool remapRoughness)
    : m_kd(kd)
    , m_ks(ks)
    , m_roughness(roughness)
    , m_remapRoughness(remapRoughness)
{
}

// PBRTv3 page 581
void PlasticMaterial::computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena) const
{
    // TODO: perform bump mapping (normal mapping)

	si.pBSDF = arena.allocate<BSDF>(si);

	// Initialize diffuse component of plastic material
	Spectrum kd = glm::clamp(m_kd->evaluate(si), 0.0f, 1.0f);
	if (!isBlack(kd))
            si.pBSDF->add(arena.allocate<LambertianReflection>(kd));

	// Initialize specular component of plastic material
	Spectrum ks = glm::clamp(m_ks->evaluate(si), 0.0f, 1.0f);
	if (!isBlack(ks)) {
		Fresnel* fresnel = arena.allocate<FresnelDielectric>(1.0f, 1.5f);

		// Create microfacet distribution for plastic material
		float rough = m_roughness->evaluate(si);
		if (m_remapRoughness)
			rough = TrowbridgeReitzDistribution::roughnessToAlpha(rough);
		MicrofacetDistribution* distrib = arena.allocate<TrowbridgeReitzDistribution>(rough, rough);

		BxDF* spec = arena.allocate<MicrofacetReflection>(ks, std::ref(*distrib), std::ref(*fresnel));
                si.pBSDF->add(spec);
	}
}

}
