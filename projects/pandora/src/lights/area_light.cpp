#include "pandora/lights/area_light.h"
#include "glm/gtc/constants.hpp"

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight, const TriangleMesh& mesh, unsigned primitiveID)
    : m_emmitedLight(emittedLight)
    , m_mesh(mesh)
    , m_primitiveID(primitiveID)
    , m_area(mesh.primitiveArea(primitiveID))
{
}

glm::vec3 AreaLight::power() const
{
    return m_emmitedLight * m_area * glm::pi<float>();
}

glm::vec3 AreaLight::light(const Interaction& interaction, const glm::vec3& w) const
{
    return glm::dot(interaction.normal, w) > 0.0f ? m_emmitedLight : glm::vec3(0.0f);
}

LightSample AreaLight::sampleLi(const Interaction& ref, const glm::vec2& randomSample) const
{
    auto [lightSample, pdf] = m_mesh.samplePrimitive(m_primitiveID, ref, randomSample);

    LightSample result;
    result.radiance = m_emmitedLight;
    result.wi = glm::normalize(lightSample.position - ref.position);
    result.pdf = pdf;
    result.visibilityRay = computeRayWithEpsilon(ref, result.wi);
    return result;
}

}
