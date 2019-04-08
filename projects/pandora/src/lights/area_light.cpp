#include "pandora/lights/area_light.h"
#include "glm/gtc/constants.hpp"
#include "pandora/graphics_core/scene.h"

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight, int numSamples, const TriangleMesh& shape, unsigned primitiveID)
    : Light((int)LightFlags::Area, numSamples)
    , m_emmitedLight(emittedLight)
    , m_shape(shape)
    , m_primitiveID(primitiveID)
    , m_area(m_shape.primitiveArea(primitiveID))
{
}

/*glm::vec3 AreaLight::power() const
{
    return m_emmitedLight * m_area * glm::pi<float>();
}*/

glm::vec3 AreaLight::light(const Interaction& interaction, const glm::vec3& w) const
{
    return glm::dot(interaction.normal, w) > 0.0f ? m_emmitedLight : glm::vec3(0.0f);
}

LightSample AreaLight::sampleLi(const Interaction& ref, const glm::vec2& randomSample) const
{
    Interaction pShape = m_shape.samplePrimitive(m_primitiveID, ref, randomSample);

    LightSample result;
    result.wi = glm::normalize(pShape.position - ref.position);
	result.pdf = m_shape.pdfPrimitive(m_primitiveID, ref, result.wi);
    result.visibilityRay = computeRayWithEpsilon(ref, pShape);
    result.radiance = light(pShape, -result.wi);
    return result;
}

float AreaLight::pdfLi(const Interaction& ref, const glm::vec3& wi) const
{
    return m_shape.pdfPrimitive(m_primitiveID, ref, wi);
}

}
