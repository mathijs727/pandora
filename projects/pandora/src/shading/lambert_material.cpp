#include "pandora/shading/lambert_material.h"
#include "glm/glm.hpp"
#include "shading.h"

namespace pandora {

LambertMaterial::LambertMaterial(glm::vec3 color)
    : m_color(color)
{
}

Material::EvalResult LambertMaterial::evalBSDF(const IntersectionData& intersection, glm::vec3 out)
{
    Material::EvalResult result;
    result.pdf = 1.0f;
    result.weigth = m_color * glm::dot(intersection.geometricNormal, out);
    return result;
}

Material::SampleResult LambertMaterial::sampleBSDF(const IntersectionData& intersection)
{

    Material::SampleResult result;
    result.out = uniformSampleHemisphere(intersection.geometricNormal);
    result.pdf = 1.0f;
    result.weight = m_color * glm::dot(intersection.geometricNormal, result.out); //intersection.incident);
    return result;
}

}