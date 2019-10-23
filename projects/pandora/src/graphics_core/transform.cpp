#include "pandora/graphics_core/transform.h"
#include "pandora/flatbuffers/data_conversion.h"
#include "pandora/utility/math.h"

namespace pandora {

// Page 93 of PBRTv3
Transform::Transform(const glm::mat4& matrix)
    : m_matrix(matrix)
    , m_inverseMatrix(glm::inverse(matrix))
{
}

Transform::Transform(const serialization::Transform* serializedTransform)
    : m_matrix(deserialize(serializedTransform->matrix()))
    , m_inverseMatrix(deserialize(serializedTransform->inverseMatrix()))
{
}

Transform Transform::operator*(const Transform& other) const
{
    return Transform(m_matrix * other.m_matrix);
}

serialization::Transform Transform::serialize() const
{
    return serialization::Transform(pandora::serialize(m_matrix), pandora::serialize(m_inverseMatrix));
}

glm::vec3 Transform::transformPoint(const glm::vec3& p) const
{
    glm::vec4 r = m_matrix * glm::vec4(p, 1.0f);
    return glm::vec3(r) / r.w;
}

glm::vec3 Transform::transformVector(const glm::vec3& v) const
{
    return glm::mat3(m_matrix) * v;
}

glm::vec3 Transform::transformNormal(const glm::vec3& n) const
{
    // Take special care with the normals so that they always orthogonal to the surface
    return glm::normalize(glm::transpose(glm::mat3(m_inverseMatrix)) * n);
}

glm::vec3 Transform::invTransformVector(const glm::vec3& v) const
{
    return glm::mat3(m_inverseMatrix) * v;
}

Ray Transform::transform(const Ray& ray) const
{
    glm::vec3 origin = m_inverseMatrix * glm::vec4(ray.origin, 1.0f);
    glm::vec3 direction = m_inverseMatrix * glm::vec4(ray.direction, 0.0f);
    return Ray(origin, direction, ray.tnear, ray.tfar);
}

Bounds Transform::transform(const Bounds& bounds) const
{
    // Transform all 8 corners
    Bounds transformedBounds;
    transformedBounds.grow(transformPoint(glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z)));

    transformedBounds.grow(transformPoint(glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z)));
    transformedBounds.grow(transformPoint(glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z)));
    return transformedBounds;
}

Interaction Transform::transform(const Interaction& si) const
{
    // https://github.com/mmp/pbrt-v3/blob/master/src/core/transform.cpp
    Interaction result;
    result.wo = glm::normalize(transformVector(si.normal));
    result.normal = glm::normalize(transformNormal(si.normal));
    result.position = transformPoint(si.position);
    return result;
}

SurfaceInteraction Transform::transform(const SurfaceInteraction& si) const
{
    // https://github.com/mmp/pbrt-v3/blob/master/src/core/transform.cpp
    SurfaceInteraction result;
    result.wo = glm::normalize(transformVector(si.normal));
    result.uv = si.uv;
    result.normal = glm::normalize(transformNormal(si.normal));
    /*result.dpdu = transformVector(si.dpdu);
    result.dpdv = transformVector(si.dpdv);
    result.dndu = transformNormal(si.dndu);
    result.dndv = transformNormal(si.dndv);*/
    result.shading.normal = glm::normalize(transformNormal(si.shading.normal));
    result.position = transformPoint(si.position);

    result.shading.normal = faceForward(result.shading.normal, result.normal);

    result.pBSDF = si.pBSDF;
    result.pSceneObject = si.pSceneObject;
    //result.primitiveID = si.primitiveID;

    return result;
}

}
