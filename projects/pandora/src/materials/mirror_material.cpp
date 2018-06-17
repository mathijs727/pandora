#include "pandora/materials/mirror_material.h"
#include "glm/glm.hpp"
#include "shading.h"

namespace pandora {

MirrorMaterial::MirrorMaterial() 
{
}

Material::EvalResult MirrorMaterial::evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const
{
    auto reflectedRay = glm::reflect(surfaceInteraction.wo, surfaceInteraction.shading.normal);

    Material::EvalResult result;
    result.pdf = 1.0f;
    result.weigth = glm::vec3(wi == reflectedRay ? 1.0f : 0.0f);
    return result;
}

Material::SampleResult MirrorMaterial::sampleBSDF(const SurfaceInteraction& surfaceInteraction, gsl::span<glm::vec2> samples) const
{
    (void)samples;

    Material::SampleResult result;
    result.out = glm::reflect(-surfaceInteraction.wo, surfaceInteraction.shading.normal);
    // Resample rays going into the geometry
    if(glm::dot(surfaceInteraction.normal, result.out) < 0.0f)
        result.out = glm::reflect(-surfaceInteraction.wo, surfaceInteraction.normal);
    result.pdf = 1.0f;
    result.multiplier = glm::vec3(1.0f);
    return result;
}

}
