
#include "pandora/lights/area_light.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight)
    : Light((int)LightFlags::Area)
    , m_emmitedLight(emittedLight)
{
}

glm::vec3 AreaLight::light(const Interaction& interaction, const glm::vec3& w) const
{
    return glm::dot(interaction.normal, w) > 0.0f ? m_emmitedLight : glm::vec3(0.0f);
}

LightSample AreaLight::sampleLi(const Interaction& ref, PcgRng& rng) const
{
    const uint32_t numPrimitives = m_pShape->numPrimitives();
    const uint32_t primitiveID = rng.uniformU32() % numPrimitives;
    Interaction pointOnShape = m_pShape->samplePrimitive(primitiveID, ref, rng);
    if (m_transform)
        pointOnShape = m_transform->transformToWorld(pointOnShape);

    LightSample result;
    result.wi = glm::normalize(pointOnShape.position - ref.position);
    result.pdf = m_pShape->pdfPrimitive(primitiveID, ref, result.wi) / static_cast<float>(numPrimitives);
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

void AreaLight::attachToShape(const Shape* pShape)
{
    m_pShape = pShape;
}

void AreaLight::attachToShape(const Shape* pShape, const glm::mat4& transform)
{
    m_pShape = pShape;
    m_transform = transform;
}

}
