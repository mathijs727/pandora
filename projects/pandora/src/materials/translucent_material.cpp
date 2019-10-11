#include "pandora/materials/translucent_material.h"
#include "glm/glm.hpp"
#include "pandora/graphics_core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/lambert_bxdf.h"
#include "reflection/microfacet.h"
#include "reflection/microfacet_bxdf.h"
#include "shading.h"

namespace pandora {
TranslucentMaterial::TranslucentMaterial(
    const std::shared_ptr<Texture<Spectrum>>& kd,
    const std::shared_ptr<Texture<Spectrum>>& ks,
    const std::shared_ptr<Texture<float>>& roughness,
    const std::shared_ptr<Texture<Spectrum>>& reflect,
    const std::shared_ptr<Texture<Spectrum>>& transmit,
    bool remapRoughness)
    : m_kd(kd)
    , m_ks(ks)
    , m_roughness(roughness)
    , m_reflect(reflect)
    , m_transmit(transmit)
    , m_remapRoughness(remapRoughness)
{
}

// Based on PBRTv3:
// https://github.com/mmp/pbrt-v3/blob/master/src/materials/translucent.cpp
void TranslucentMaterial::computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena) const
{
    // TODO: perform bump mapping (normal mapping)

    float eta = 1.5f;

    // Evaluate textures and allocate BRDF
    si.pBSDF = arena.allocate<BSDF>(si);

    Spectrum r = glm::clamp(m_reflect->evaluate(si), 0.0f, 1.0f);
    Spectrum t = glm::clamp(m_transmit->evaluate(si), 0.0f, 1.0f);
    if (isBlack(r) && isBlack(t))
        return;

    Spectrum kd = glm::clamp(m_kd->evaluate(si), 0.0f, 1.0f);
    if (!isBlack(kd)) {
        if (!isBlack(r))
            si.pBSDF->add(arena.allocate<LambertianReflection>(r * kd));
        if (!isBlack(t))
            si.pBSDF->add(arena.allocate<LambertianTransmission>(t * kd));
    }

    // TODO: specular (https://github.com/mmp/pbrt-v3/blob/master/src/materials/translucent.cpp)
    Spectrum ks = glm::clamp(m_ks->evaluate(si), 0.0f, 1.0f);
    if (!isBlack(ks) && (!isBlack(r) || !isBlack(t))) {
        float rough = m_roughness->evaluate(si);
        if (m_remapRoughness) {
            rough = TrowbridgeReitzDistribution::roughnessToAlpha(rough);
        }
        MicrofacetDistribution* distrib = arena.allocate<TrowbridgeReitzDistribution>(rough, rough);
        if (!isBlack(r)) {
            Fresnel* fresnel = arena.allocate<FresnelDielectric>(1.0f, eta);
            si.pBSDF->add(
                arena.allocate<MicrofacetReflection>(
                    r * ks,
                    std::reference_wrapper(*distrib),
                    std::reference_wrapper(*fresnel)));
        }
        if (!isBlack(t)) {
            si.pBSDF->add(
                arena.allocate<MicrofacetTransmission>(
                    t * ks,
                    std::reference_wrapper(*distrib),
                    1.0f,
                    eta));
        }
    }
}
}
