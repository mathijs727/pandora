#include "pandora/graphics_core/transform.h"
#include "pandora/utility/math.h"

namespace pandora {

// Page 93 of PBRTv3
Transform::Transform(const glm::mat4& matrix)
    : m_matrix(matrix)
    , m_inverseMatrix(glm::inverse(matrix))
{
}

Transform Transform::operator*(const Transform& other) const
{
    return Transform(m_matrix * other.m_matrix);
}

glm::vec3 Transform::transformPointToWorld(const glm::vec3& p) const
{
    glm::vec4 r = m_matrix * glm::vec4(p, 1.0f);
    return glm::vec3(r) / r.w;
}

glm::vec3 Transform::transformVectorToWorld(const glm::vec3& v) const
{
    return glm::mat3(m_matrix) * v;
}

glm::vec3 Transform::transformNormalToWorld(const glm::vec3& n) const
{
    // Take special care with the normals so that they always orthogonal to the surface
    return glm::normalize(glm::transpose(glm::mat3(m_inverseMatrix)) * n);
}

glm::vec3 Transform::transformVectorToLocal(const glm::vec3& v) const
{
    return glm::mat3(m_inverseMatrix) * v;
}

RayHit Transform::transformToLocal(const RayHit& hit) const
{
    RayHit result = hit;
    //result.geometricNormal = glm::normalize(glm::transpose(glm::mat3(m_matrix)) * hit.geometricNormal);
    return result;
}

Ray Transform::transformToLocal(const Ray& ray) const
{
    const glm::vec3 origin = m_inverseMatrix * glm::vec4(ray.origin, 1.0f);
    const glm::vec3 direction = m_inverseMatrix * glm::vec4(ray.direction, 0.0f);
    return Ray(origin, direction, ray.tnear, ray.tfar);
}

Bounds Transform::transformToWorld(const Bounds& bounds) const
{
    // Transform all 8 corners
    Bounds transformedBounds;
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z)));

    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z)));
    transformedBounds.grow(transformPointToWorld(glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z)));
    return transformedBounds;
}

Interaction Transform::transformToWorld(const Interaction& si) const
{
    // https://github.com/mmp/pbrt-v3/blob/master/src/core/transform.cpp
    Interaction result;
    result.position = transformPointToWorld(si.position);
    result.normal = glm::normalize(transformNormalToWorld(si.normal));
    result.wo = glm::normalize(transformVectorToWorld(si.normal));
    return result;
}

SurfaceInteraction Transform::transformToWorld(const SurfaceInteraction& si) const
{
    // https://github.com/mmp/pbrt-v3/blob/master/src/core/transform.cpp
    SurfaceInteraction result = si;
    result.position = transformPointToWorld(si.position);
    result.normal = transformNormalToWorld(si.normal);

    result.shading.normal = transformNormalToWorld(si.shading.normal);
    result.shading.normal = faceForward(result.shading.normal, result.normal);

    result.wo = glm::normalize(transformVectorToWorld(si.wo));
    return result;
}

}
