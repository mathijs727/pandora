#include "pandora/lights/area_light.h"
#include "glm/gtc/constants.hpp"
#include "pandora/core/scene.h"

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight, int numSamples, const SceneObject& sceneObject, unsigned primitiveID)
    : Light((int)LightFlags::Area, numSamples)
    , m_emmitedLight(emittedLight)
    , m_sceneObject(sceneObject)
    , m_primitiveID(primitiveID)
    , m_area(sceneObject.getMesh().primitiveArea(primitiveID))
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
    auto [lightSample, pdf] = m_sceneObject.getMesh().samplePrimitive(m_primitiveID, ref, randomSample);

    LightSample result;
    result.radiance = m_emmitedLight;
    result.wi = glm::normalize(lightSample.position - ref.position);
    result.pdf = pdf;
    result.visibilityRay = computeRayWithEpsilon(ref, lightSample);
    return result;
}

}
