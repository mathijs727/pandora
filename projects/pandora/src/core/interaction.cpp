#include "pandora/core/interaction.h"
#include "pandora/core/bxdf.h"
#include "pandora/core/material.h"
#include "pandora/core/scene.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

void SurfaceInteraction::setShadingGeometry(
    const glm::vec3& dpdus,
    const glm::vec3& dpdvs,
    const glm::vec3& dndus,
    const glm::vec3& dndvs,
    bool orientationIsAuthoritative)
{
    // Compute shading normal
    shading.normal = glm::normalize(glm::cross(dpdus, dpdvs));
    if (orientationIsAuthoritative)
        normal = faceForward(normal, shading.normal);
    else
        shading.normal = faceForward(shading.normal, normal);

    shading.dpdu = dpdus;
    shading.dpdv = dpdvs;
    shading.dndu = dndus;
    shading.dndv = dndvs;
}

void SurfaceInteraction::computeScatteringFunctions(const Ray& ray, MemoryArena& arena, TransportMode mode, bool allowMultipleLobes)
{
    // TODO: compute ray differentials

    sceneObject->material->computeScatteringFunctions(*this, arena, mode, allowMultipleLobes);
}

glm::vec3 SurfaceInteraction::lightEmitted(const glm::vec3& w) const
{
    if (sceneObject->areaLightPerPrimitive) {
        const auto& area = sceneObject->areaLightPerPrimitive[primitiveID];
        return area->light(*this, w);
    } else {
        return glm::vec3(0.0f);
    }
}

Ray SurfaceInteraction::spawnRay(const glm::vec3& dir) const
{
    return computeRayWithEpsilon(*this, dir);
}

// PBRTv3 page 231
glm::vec3 offsetRayOrigin(const Interaction& i1, const glm::vec3& dir)
{
     glm::vec3 error(RAY_EPSILON);

    float d = glm::dot(glm::abs(i1.normal), error);
    glm::vec3 offset = d * i1.normal;
    if (glm::dot(i1.normal, dir) < 0.0f)
        offset = -offset;

    glm::vec3 po = i1.position + offset;
    // Round offset point p0 away from p
    for (int i = 0;i  < 3; i++)
    {
        if (offset[i] > 0.0f)
            po[i] = nextFloatUp(po[i]);
        else if (offset[i] < 0.0f)
            po[i] = nextFloatDown(po[i]);
    }
    return po;
}


Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i2)
{
    glm::vec3 start = offsetRayOrigin(i1, i2.position - i1.position);
    glm::vec3 end = offsetRayOrigin(i2, i1.position - i2.position);

    glm::vec3 direction = glm::normalize(i2.position - i1.position);
    return Ray(start, direction, 0.0f, (start - end).length() - RAY_EPSILON);// Extra epsilon just to make sure
}

Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir)
{
   return Ray(offsetRayOrigin(i1, dir), dir);
}

}
