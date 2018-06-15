#include "pandora/shading/lambert_material.h"
#include "glm/glm.hpp"
#include "shading.h"

namespace pandora {

LambertMaterial::LambertMaterial(std::shared_ptr<Texture> texture)
    : m_colorTexture(texture)
{
}

Material::EvalResult LambertMaterial::evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const
{
    Material::EvalResult result;
    result.pdf = 1.0f;
    result.weigth = m_colorTexture->evaluate(surfaceInteraction) * glm::dot(surfaceInteraction.shading.normal, wi);
    return result;
}

Material::SampleResult LambertMaterial::sampleBSDF(const SurfaceInteraction& surfaceInteraction) const
{
    Material::SampleResult result;
    result.out = uniformSampleHemisphere(surfaceInteraction.normal);
    result.pdf = 1.0f;
    result.weight = m_colorTexture->evaluate(surfaceInteraction) * glm::dot(surfaceInteraction.shading.normal, result.out);
    return result;
}

}
