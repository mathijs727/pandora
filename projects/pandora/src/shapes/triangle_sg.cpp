#pragma once
#include "pandora/shapes/triangle.h"
#include "pandora/utility/math.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

static glm::mat4 assimpMatrix(const aiMatrix4x4& m)
{
    //float values[3][4] = {};
    glm::mat4 matrix;
    matrix[0][0] = m.a1;
    matrix[0][1] = m.b1;
    matrix[0][2] = m.c1;
    matrix[0][3] = m.d1;
    matrix[1][0] = m.a2;
    matrix[1][1] = m.b2;
    matrix[1][2] = m.c2;
    matrix[1][3] = m.d2;
    matrix[2][0] = m.a3;
    matrix[2][1] = m.b3;
    matrix[2][2] = m.c3;
    matrix[2][3] = m.d3;
    matrix[3][0] = m.a4;
    matrix[3][1] = m.b4;
    matrix[3][2] = m.c4;
    matrix[3][3] = m.d4;
    return matrix;
}

static glm::vec3 assimpVec(const aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

namespace pandora {

TriangleShadingGeometry::TriangleShadingGeometry(
    const TriangleIntersectGeometry* pIntersectGeometry, std::vector<glm::vec3>&& normals, std::vector<glm::vec2>&& uvCoords)
    : m_pIntersectGeometry(pIntersectGeometry)
    , m_normals(std::move(normals))
    , m_uvCoords(std::move(uvCoords))
{
    m_normals.shrink_to_fit();
    m_uvCoords.shrink_to_fit();
}

SurfaceInteraction pandora::TriangleShadingGeometry::fillSurfaceInteraction(const Ray& ray, const RayHit& hitInfo) const
{
    glm::ivec3 triangle = m_pIntersectGeometry->m_indices[hitInfo.primitiveID];
    glm::vec3 p0 = m_pIntersectGeometry->m_positions[triangle[0]];
    glm::vec3 p1 = m_pIntersectGeometry->m_positions[triangle[1]];
    glm::vec3 p2 = m_pIntersectGeometry->m_positions[triangle[2]];

#if PBRT_INTERSECTION > 0
    // Compute barycentric coordinates and t value for triangle intersection
    float b0 = hitInfo.geometricUV.x;
    float b1 = hitInfo.geometricUV.y;
    float b2 = std::max(0.0f, std::min(1.0f, 1.0f - hitInfo.geometricUV.x - hitInfo.geometricUV.y));
    assert(b2 >= 0.0f && b2 <= 1.0f);
#else
    float b0 = 1 - hitInfo.geometricUV.x - hitInfo.geometricUV.y;
    float b1 = hitInfo.geometricUV.x;
    float b2 = hitInfo.geometricUV.y;
#endif

    // Compute triangle partial derivatives
    glm::vec3 dpdu, dpdv;
    glm::vec2 uv[3];
    getUVs(hitInfo.primitiveID, uv);
    // Compute deltas for triangle partial derivatives
    glm::vec2 duv02 = uv[0] - uv[2], duv12 = uv[1] - uv[2];
    glm::vec3 dp02 = p0 - p2, dp12 = p1 - p2;

    float determinant = duv02[0] * duv12[1] - duv02[1] * duv12[0];
    if (determinant == 0.0f) {
        // Handle zero determinant for triangle partial derivative matrix
        coordinateSystem(glm::normalize(glm::cross(p2 - p0, p1 - p0)), &dpdu, &dpdv);
    } else {
        float invDet = 1.0f / determinant;
        dpdu = (duv12[1] * dp02 - duv02[1] * dp12) * invDet;
        dpdv = (-duv12[0] * dp02 + duv02[0] * dp12) * invDet;
    }

    // TODO: compute error bounds for triangle intersection

    // Interpolate (u, v) parametric coordinates and hit point
    glm::vec3 pHit = b0 * p0 + b1 * p1 + b2 * p2;
    glm::vec2 uvHit = b0 * uv[0] + b1 * uv[1] + b2 * uv[2];

    // TODO: test intersection against alpha texture, if present

    // Fill in  surface interaction from triangle hit
    auto isect = SurfaceInteraction(pHit, uvHit, -ray.direction, dpdu, dpdv, glm::vec3(0.0f), glm::vec3(0.0f)); //, hitInfo.primitiveID);

    // Override surface normal in isect for triangle
    isect.normal = isect.shading.normal = glm::normalize(glm::cross(dp02, dp12));

    // Shading normals / tangents
    if (!m_normals.empty() || !m_tangents.empty()) {
        glm::ivec3 v = m_pIntersectGeometry->m_indices[hitInfo.primitiveID];

        // Compute shading normal ns for triangle
        glm::vec3 ns;
        if (!m_normals.empty())
            ns = glm::normalize(b0 * m_normals[v[0]] + b1 * m_normals[v[1]] + b2 * m_normals[v[2]]);
        else
            ns = isect.normal;

        // Compute shading tangent ss for triangle
        glm::vec3 ss;
        if (!m_tangents.empty())
            ss = glm::normalize(b0 * m_tangents[v[0]] + b1 * m_tangents[v[1]] + b2 * m_tangents[v[2]]);
        else
            ss = glm::normalize(isect.dpdu);

        // Compute shading bitangent ts for triangle and adjust ss
        glm::vec3 ts = glm::cross(ss, ns);
        if (glm::dot(ts, ts) > 0.0f) { // glm::dot(ts, ts) = length2(ts)
            ts = glm::normalize(ts);
            ss = glm::cross(ts, ns);
        } else {
            coordinateSystem(ns, &ss, &ts);
        }

        // Compute dndu and dndv for triangle shading geometry
        glm::vec3 dndu, dndv;
        if (!m_normals.empty()) {
            //glm::vec2 duv02 = uv[0] - uv[2];
            //glm::vec2 duv12 = uv[1] - uv[2];
            glm::vec3 dn1 = m_normals[v[0]] - m_normals[v[2]];
            glm::vec3 dn2 = m_normals[v[1]] - m_normals[v[2]];
            float determinant = duv02[0] * duv12[1] - duv02[1] * duv12[0];
            bool degenerateUV = std::abs(determinant) < 1e-8;
            if (degenerateUV) {
                dndu = dndv = glm::vec3(0.0f);
            } else {
                float invDet = 1.0f / determinant;
                dndu = (duv12[1] * dn1 - duv02[1] * dn2) * invDet;
                dndv = (-duv12[0] * dn1 + duv02[0] * dn2) * invDet;
            }
        } else {
            dndu = dndv = glm::vec3(0.0f);
        }
        isect.setShadingGeometry(ss, ts, dndu, dndv, true);
    }

    // Ensure correct orientation of the geometric normal
    if (!m_normals.empty())
        isect.normal = faceForward(isect.normal, isect.shading.normal);
    //else if (reverseOrientation ^ transformSwapsHandedness)
    //	isect.normal = isect.shading.normal = -isect.normal;

    isect.wo = -ray.direction;
    //isect.primitiveID = hitInfo.primitiveID;
    return isect;
}

void TriangleShadingGeometry::getUVs(unsigned primitiveID, gsl::span<glm::vec2, 3> uv) const
{
    if (!m_uvCoords.empty()) {
        glm::ivec3 indices = m_pIntersectGeometry->m_indices[primitiveID];
        uv[0] = m_uvCoords[indices[0]];
        uv[1] = m_uvCoords[indices[1]];
        uv[2] = m_uvCoords[indices[2]];
    } else {
        uv[0] = glm::vec2(0, 0);
        uv[1] = glm::vec2(1, 0);
        uv[2] = glm::vec2(1, 1);
    }
}

}