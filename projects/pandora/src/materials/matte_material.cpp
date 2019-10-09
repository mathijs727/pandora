#include "pandora/materials/matte_material.h"
#include "glm/glm.hpp"
#include "pandora/graphics_core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/lambert_bxdf.h"
#include "reflection/oren_nayer_bxdf.h"
#include "shading.h"

namespace pandora {

MatteMaterial::MatteMaterial(const std::shared_ptr<Texture<Spectrum>>& kd, const std::shared_ptr<Texture<float>>& sigma)
    : m_kd(kd)
    , m_sigma(sigma)
{
}

void MatteMaterial::computeScatteringFunctions(SurfaceInteraction& si, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes) const
{
    // TODO: perform bump mapping (normal mapping)

    // Evaluate textures and allocate BRDF
    si.pBSDF = arena.allocate<BSDF>(si);
    Spectrum r = glm::clamp(m_kd->evaluate(si), 0.0f, 1.0f);
    float sigma = std::clamp(m_sigma->evaluate(si), 0.0f, 90.0f);
    if (!isBlack(r)) {
        if (sigma == 0.0f) {
            si.pBSDF->add(arena.allocate<LambertianReflection>(r));
        } else {
            si.pBSDF->add(arena.allocate<OrenNayerBxDF>(r, sigma));
        }
    }

}

}
