#include "pandora/lights/area_light.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/shapes/triangle.h"
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

namespace pandora {

AreaLight::AreaLight(glm::vec3 emittedLight, const Shape* pShape)
    : Light((int)LightFlags::Area)
    , m_emmitedLight(emittedLight)
    , m_pShape(pShape)
{
}

glm::vec3 AreaLight::light(const Interaction& interaction, const glm::vec3& w) const
{
    return glm::dot(interaction.normal, w) > 0.0f ? m_emmitedLight : glm::vec3(0.0f);
}

LightSample AreaLight::sampleLi(const Interaction& ref, PcgRng& rng) const
{
    const auto* intersectGeom = m_pShape->getIntersectGeometry();

	const uint32_t numPrimitives = m_pShape->numPrimitives();
    const uint32_t primitiveID = rng.uniformU32() % numPrimitives;
    const Interaction pointOnShape = intersectGeom->samplePrimitive(primitiveID, ref, rng);

    LightSample result;
    result.wi = glm::normalize(pointOnShape.position - ref.position);
    result.pdf = intersectGeom->pdfPrimitive(primitiveID, ref, result.wi);
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
