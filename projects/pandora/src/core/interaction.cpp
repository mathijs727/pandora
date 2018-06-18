#include "pandora/core/interaction.h"
#include "pandora/core/scene.h"
#include "pandora/utility/math.h"

namespace pandora {

void SurfaceInteraction::setShadingGeometry(
    const glm::vec3& dpdus,
    const glm::vec3& dpdvs,
    const glm::vec3& dndus,
    glm::vec3& dndvs,
    bool orientationIsAuthoritative)
{
    // Compute shading normal
    shading.normal = glm::normalize(glm::cross(dpdus, dpdvs));
    if (orientationIsAuthoritative)
        normal = faceForward(normal ,shading.normal);
    else
        shading.normal = faceForward(shading.normal, normal);

    shading.dpdu = dpdus;
    shading.dpdv = dpdvs;
    shading.dndu = dndus;
    shading.dndv = dndvs;
}

glm::vec3 SurfaceInteraction::lightEmitted(const glm::vec3& w) const
{
    if (sceneObject->areaLightPerPrimitive) {
        const auto& area = sceneObject->areaLightPerPrimitive[primitiveID];
        return area.light(*this, w);
    } else {
        return glm::vec3(0.0f);
    }
}

Ray computeRayWithEpsilon(const Interaction& i1, const Interaction& i2)
{
    glm::vec3 direction = i2.position - i1.position;

    glm::vec3 start = glm::dot(i1.normal, direction) > 0.0f ? i1.position + i1.normal * RAY_EPSILON : i1.position - i1.normal * RAY_EPSILON;
    glm::vec3 end = glm::dot(i2.normal, -direction) > 0.0f ? i2.position + i2.normal * RAY_EPSILON : i2.position - i2.normal * RAY_EPSILON;

    return Ray(start, glm::normalize(end - start));
}

Ray computeRayWithEpsilon(const Interaction& i1, const glm::vec3& dir)
{
    if (glm::dot(i1.normal, dir) > 0.0f)
        return Ray(i1.position + i1.normal * RAY_EPSILON, dir);
    else
        return Ray(i1.position - i1.normal * RAY_EPSILON, dir);
}

}
