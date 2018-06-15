#include "pandora/shading/lambert_material.h"
#include "glm/glm.hpp"
#include "shading.h"

namespace pandora {

LambertMaterial::LambertMaterial(std::shared_ptr<Texture> texture)
    : m_colorTexture(texture)
{
}

Material::EvalResult LambertMaterial::evalBSDF(const IntersectionData& intersection, glm::vec3 out) const
{
    Material::EvalResult result;
    result.pdf = 1.0f;
    result.weigth = m_colorTexture->evaluate(intersection) * glm::dot(intersection.shadingNormal, out);
    return result;
}

Material::SampleResult LambertMaterial::sampleBSDF(const IntersectionData& intersection) const
{

    Material::SampleResult result;
    result.out = uniformSampleHemisphere(intersection.geometricNormal);
    result.pdf = 1.0f;
    result.weight = m_colorTexture->evaluate(intersection) * glm::dot(intersection.shadingNormal, result.out);
    return result;
}

}
