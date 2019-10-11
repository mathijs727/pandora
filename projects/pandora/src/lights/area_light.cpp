#include "pandora/lights/area_light.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight, int numSamples, const TriangleShape& shape, unsigned primitiveID)
    : Light((int)LightFlags::Area, numSamples)
    , m_emmitedLight(emittedLight)
    , m_shape(shape)
    , m_primitiveID(primitiveID)
    , m_area(m_shape.getIntersectGeometry()->primitiveArea(primitiveID))
{
}

glm::vec3 AreaLight::light(const Interaction& interaction, const glm::vec3& w) const
{
    return glm::dot(interaction.normal, w) > 0.0f ? m_emmitedLight : glm::vec3(0.0f);
}

LightSample AreaLight::sampleLi(const Interaction& ref, const glm::vec2& randomSample) const
{
    const auto* intersectGeom = m_shape.getIntersectGeometry();
    Interaction pointOnShape = intersectGeom->samplePrimitive(m_primitiveID, ref, randomSample);

    LightSample result;
    result.wi = glm::normalize(pointOnShape.position - ref.position);
    spdlog::warn("AreaLight::sampleLi result.pdf = 0.0f; because pdfPrimitive with direction is not implemented");
    result.pdf = 0.0f; //intersectGeom->pdfPrimitive(m_primitiveID, ref, result.wi);
    result.visibilityRay = computeRayWithEpsilon(ref, pointOnShape);
    result.radiance = light(pointOnShape, -result.wi);
    return result;
}

float AreaLight::pdfLi(const Interaction& ref, const glm::vec3& wi) const
{
    //const auto* intersectGeom = m_shape.getIntersectGeometry();
    //return intersectGeom->pdfPrimitive(m_primitiveID, ref, wi);
    spdlog::warn("AreaLight::pdfLi returns 0.0f because pdfPrimitive with direction is not implemented");
    return 0.0f;
}

}
