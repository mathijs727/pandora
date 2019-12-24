
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/lights/area_light.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

SurfaceInteraction::SurfaceInteraction(
    const glm::vec3& p, const glm::vec3& normal, const glm::vec2& uv, const glm::vec3& wo)
    : Interaction(p, normal, wo)
    , uv(uv)
{
    // Initialize shading geometry from true geometry
    shading.normal = normal;

    // TODO: adjust normal based on orientation and handedness
}

// PBRTv3 page 232
Ray Interaction::spawnRay(const glm::vec3& d) const
{
    return computeRayWithEpsilon(*this, d);
}

glm::vec3 SurfaceInteraction::Le(const glm::vec3& w) const
{
    const AreaLight* pAreaLight = pSceneObject->pAreaLight;
    if (!pAreaLight)
        return glm::vec3(0);

    return pAreaLight->light(*this, w);
}

// PBRTv3 page 231
glm::vec3 offsetRayOrigin(const Interaction& i1, const glm::vec3& dir)
{
    if (glm::dot(i1.normal, dir) > 0.0f) { // Faster
        return i1.position + i1.normal * RAY_EPSILON;
    } else {
        return i1.position - i1.normal * RAY_EPSILON;
    }
}

Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i2)
{
    glm::vec3 direction = glm::normalize(i2.position - i1.position);
    glm::vec3 start = offsetRayOrigin(i1, direction);
    glm::vec3 end = offsetRayOrigin(i2, -direction);

    return Ray(start, direction, 0, glm::length(start - end)); // Extra epsilon just to make sure
}

Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir)
{
    return Ray(offsetRayOrigin(i1, dir), dir);
}

void SurfaceInteraction::setShadingGeometry(const glm::vec3& shadingNormal, const glm::vec2& textureCoords)
{
    shading.normal = shadingNormal;
    shading.st = textureCoords;
}

void SurfaceInteraction::computeScatteringFunctions(const Ray& ray, MemoryArena& arena)
{
    // TODO: compute ray differentials
    pSceneObject->pMaterial->computeScatteringFunctions(*this, arena);
    assert(pBSDF);
}

}

/*// PBRTv3 page 119
SurfaceInteraction::SurfaceInteraction(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wo, const glm::vec3& dpdu, const glm::vec3& dpdv, const glm::vec3& dndu, const glm::vec3& dndv, const TriangleMesh* shape, unsigned primitiveID)
    : Interaction(p, glm::normalize(glm::cross(dpdu, dpdv)), wo)
    , uv(uv)
    , dpdu(dpdu)
    , dpdv(dpdv)
    , dndu(dndu)
    , dndv(dndv)
    , shape(shape)
    , primitiveID(primitiveID)
{
    // Initialize shading geometry from true geometry
    shading.normal = normal;
    shading.dpdu = dpdu;
    shading.dpdv = dpdv;
    shading.dndu = dndu;
    shading.dndv = dndv;

    // TODO: adjust normal based on orientation and handedness
}

void SurfaceInteraction::computeScatteringFunctions(const Ray& ray, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes)
{
    // TODO: compute ray differentials
    material->computeScatteringFunctions(*this, arena, mode, allowMultipleLobes);
    assert(this->bsdf != nullptr);
}

inline SurfaceInteraction::SurfaceInteraction(const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wo, const glm::vec3& dpdu, const glm::vec3& dpdv, const glm::vec3& dndu, const glm::vec3& dndv, unsigned primitiveID)
    : Interaction(p, glm::normalize(glm::cross(dpdu, dpdv)), wo)
    , uv(uv)
    , dpdu(dpdu)
    , dpdv(dpdv)
    , dndu(dndu)
    , dndv(dndv)
{
    // Initialize shading geometry from true geometry
    shading.normal = normal;
    shading.dpdu = dpdu;
    shading.dpdv = dpdv;
    shading.dndu = dndu;
    shading.dndv = dndv;

    // TODO: adjust normal based on orientation and handedness
}
*/
