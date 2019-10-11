
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/lights/area_light.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

SurfaceInteraction::SurfaceInteraction(
    const glm::vec3& p, const glm::vec2& uv, const glm::vec3& wo, const glm::vec3& dpdu, const glm::vec3& dpdv, const glm::vec3& dndu, const glm::vec3& dndv)
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

// PBRTv3 page 232
Ray Interaction::spawnRay(const glm::vec3& d) const
{
    return computeRayWithEpsilon(*this, d);
}

glm::vec3 SurfaceInteraction::Le(const glm::vec3& w) const
{
    const AreaLight* pAreaLight = pSceneObject->pAreaLight.get();
    if (!pAreaLight)
        return glm::vec3(0);

    // Transform from world to local direction
    const Transform* pTransform = nullptr;// sceneObject.pTransform;
    glm::vec3 wLocal = pTransform ? pTransform->invTransformVector(w) : w;

    return pAreaLight->light(*this, wLocal);
}

// PBRTv3 page 231
glm::vec3 offsetRayOrigin(const Interaction& i1, const glm::vec3& dir)
{
    // More accurate if we'd actually go through the trouble of keeping track of the error
    /*glm::vec3 error(RAY_EPSILON);

    float d = glm::dot(glm::abs(i1.normal), error);
    glm::vec3 offset = d * i1.normal;
    if (glm::dot(i1.normal, dir) < 0.0f)
        offset = -offset;

    glm::vec3 po = i1.position + offset;
    // Round offset point p0 away from p
    for (int i = 0; i < 3; i++) {
        if (offset[i] > 0.0f)
            po[i] = nextFloatUp(po[i]);
        else if (offset[i] < 0.0f)
            po[i] = nextFloatDown(po[i]);
    }
    return po;*/

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

void SurfaceInteraction::setShadingGeometry(
    const glm::vec3& dpdus,
    const glm::vec3& dpdvs,
    const glm::vec3& dndus,
    const glm::vec3& dndvs,
    bool orientationIsAuthoritative)
{
    // Compute shading normal
    shading.normal = glm::normalize(glm::cross(dpdus, dpdvs));
    // TODO: adjust normal based on orientation and handedness (page 119)
    if (orientationIsAuthoritative)
        normal = faceForward(normal, shading.normal);
    else
        shading.normal = faceForward(shading.normal, normal);

    // Initialize shading partial derivative values
    shading.dpdu = dpdus;
    shading.dpdv = dpdvs;
    shading.dndu = dndus;
    shading.dndv = dndvs;
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
