#include "pandora/materials/matte_material.h"
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/utility/memory_arena.h"
#include "reflection/lambert_reflection.h"
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
    si.bsdf = arena.allocate<BSDF>(si);
    Spectrum r = glm::clamp(m_kd->evaluate(si), 0.0f, 1.0f);
    float sigma = std::clamp(m_sigma->evaluate(si), 0.0f, 90.0f);
    if (!isBlack(r)) {
        if (sigma == 0.0f) {
            si.bsdf->add(arena.allocate<LambertianReflection>(r));
        } else {
            assert(false);// Not implemented yet!
        }
    }

}

/*Material::EvalResult LambertMaterial::evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const
{
    Material::EvalResult result;
    result.pdf = 1.0f;
    result.weigth = m_colorTexture->evaluate(surfaceInteraction) * glm::dot(surfaceInteraction.shading.normal, wi);
    return result;
}

Material::SampleResult LambertMaterial::sampleBSDF(const SurfaceInteraction& surfaceInteraction, gsl::span<glm::vec2> samples) const
{
    Material::SampleResult result;
    result.out = uniformSampleHemisphere(surfaceInteraction.normal, samples[0]);
    result.pdf = 1.0f;
    result.multiplier = m_colorTexture->evaluate(surfaceInteraction) * glm::dot(surfaceInteraction.shading.normal, result.out);
    return result;
}*/

}
