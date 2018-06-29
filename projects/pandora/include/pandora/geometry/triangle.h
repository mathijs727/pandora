#pragma once
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/core/material.h"
#include "pandora/geometry/bounds.h"
#include "pandora/utility/math.h"
#include <gsl/span>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

struct aiScene;

namespace pandora {

class TriangleMesh {
public:
    // Public constructor because std::make_shared does not work on private constructors
    TriangleMesh(unsigned numTriangles,
        unsigned numVertices,
        std::unique_ptr<glm::ivec3[]>&& triangles,
        std::unique_ptr<glm::vec3[]>&& positions,
        std::unique_ptr<glm::vec3[]>&& normals,
        std::unique_ptr<glm::vec3[]>&& tangents,
        std::unique_ptr<glm::vec2[]>&& uvCoords);
    ~TriangleMesh() = default;

    static std::vector<std::shared_ptr<TriangleMesh>> loadFromFile(const std::string_view filename, glm::mat4 transform = glm::mat4(1), bool ignoreVertexNormals = false);

    unsigned numTriangles() const;
    unsigned numVertices() const;

    gsl::span<const glm::ivec3> getTriangles() const;
    gsl::span<const glm::vec3> getPositions() const;

    // Temporary, to work with native Embree intersection kernels.
    // TODO: Replace this (and the getters above) with a primitive abstraction (bounds + split + intersect) so the Embree back-end uses the same intersection code as my own (TODO) kernels.
    // This abstraction should be higher performance than the shared_ptr for each triangle approach that PBRTv3 takes.
    //SurfaceInteraction partialFillSurfaceInteraction(unsigned primID, const glm::vec2& hitUV) const;// Caller should initialize sceneObject & wo

    Bounds getPrimitiveBounds(unsigned primitiveID) const;

    __forceinline bool intersectPrimitive(unsigned primitiveID, const Ray& ray, float& tHit, SurfaceInteraction& isect, bool testAlphaTexture = true) const;

    float primitiveArea(unsigned primitiveID) const;
    Interaction samplePrimitive(unsigned primitiveID, const glm::vec2& randomSample) const;
    Interaction samplePrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec2& randomSample) const;

    float pdfPrimitive(unsigned primitiveID, const Interaction& ref) const;
    float pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const;

private:
    static std::shared_ptr<TriangleMesh> createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& transform, bool ignoreVertexNormals);

    void getUVs(unsigned primitiveID, gsl::span<glm::vec2, 3> uv) const;
    void getPs(unsigned primitiveID, gsl::span<glm::vec3, 3> p) const;

private:
    const unsigned m_numTriangles, m_numVertices;

    std::unique_ptr<glm::ivec3[]> m_triangles;
    const std::unique_ptr<glm::vec3[]> m_positions;
    const std::unique_ptr<glm::vec3[]> m_normals;
    const std::unique_ptr<glm::vec3[]> m_tangents;
    const std::unique_ptr<glm::vec2[]> m_uvCoords;
};

__forceinline bool TriangleMesh::intersectPrimitive(unsigned primitiveID, const Ray& ray, float& tHit, SurfaceInteraction& isect, bool testAlphaTexture) const
{
#if ROBUST_INTERSECTION > 0
    // Based on PBRT v3 triangle intersection test (page 158):
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    //
    // Transform the ray and triangle such that the ray origin is at (0,0,0) and its
    // direction points along the +Z axis. This makes the intersection test easy and
    // allows for watertight intersection testing.
    glm::ivec3 triangle = m_triangles[primitiveID];
    glm::vec3 p0 = m_positions[triangle[0]];
    glm::vec3 p1 = m_positions[triangle[1]];
    glm::vec3 p2 = m_positions[triangle[2]];

    // Translate vertices based on ray origin
    glm::vec3 p0t = p0 - ray.origin;
    glm::vec3 p1t = p1 - ray.origin;
    glm::vec3 p2t = p2 - ray.origin;

    // Permutate components of triangle vertices and ray direction
    int kz = maxDimension(glm::abs(ray.direction));
    int kx = kz + 1;
    if (kx == 3)
        kx = 0;
    int ky = kx + 1;
    if (ky == 3)
        ky = 0;
    glm::vec3 d = permute(ray.direction, kx, ky, kz);
    p0t = permute(p0t, kx, ky, kz);
    p1t = permute(p1t, kx, ky, kz);
    p2t = permute(p2t, kx, ky, kz);

    // Apply shear transformation to translated vertex positions.
    // Aligns the ray direction with the +z axis. Only shear x and y dimensions,
    // we can wait and shear the z dimension only if the ray actually intersects
    // the triangle.
    //TODO(Mathijs): consider precomputing and storing the shear values in the ray.
    float Sx = -d.x / d.z;
    float Sy = -d.y / d.z;
    float Sz = 1.0f / d.z;
    p0t.x += Sx * p0t.z;
    p0t.y += Sy * p0t.z;
    p1t.x += Sx * p1t.z;
    p1t.y += Sy * p1t.z;
    p2t.x += Sx * p2t.z;
    p2t.y += Sy * p2t.z;

    // Compute edge function coefficients
    float e0 = p1t.x * p2t.y - p1t.y * p2t.x;
    float e1 = p2t.x * p0t.y - p2t.y * p0t.x;
    float e2 = p0t.x * p1t.y - p0t.y * p1t.x;

    // Fall back to double precision test at triangle edges
    if (e0 == 0.0f || e1 == 0.0f || e2 == 0.0f) {
        double p2txp1ty = (double)p2t.x * (double)p1t.y;
        double p2typ1tx = (double)p2t.y * (double)p1t.x;
        e0 = (float)(p2typ1tx - p2txp1ty);
        double p0txp2ty = (double)p0t.x * (double)p2t.y;
        double p0typ2tx = (double)p0t.y * (double)p2t.x;
        e1 = (float)(p0typ2tx - p0txp2ty);
        double p1txp0ty = (double)p1t.x * (double)p0t.y;
        double p1typ0tx = (double)p1t.y * (double)p0t.x;
        e2 = (float)(p1typ0tx - p1txp0ty);
    }

    // If the signs of the edge function values differ, then the point (0, 0) is not
    // on the same side of all three edges and therefor is outside the triangle.
    if ((e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) && ((e0 > 0.0f || e1 > 0.0f || e2 > 0.0f)))
        return false;

    // If the sum of the three ege function values is zero, then the ray is
    // approaching the triangle edge-on, and we report no intersection.
    float det = e0 + e1 + e2;
    if (det == 0.0f)
        return false;

    // Compute scaled hit distance to triangle and test against t range
    p0t.z *= Sz;
    p1t.z *= Sz;
    p2t.z *= Sz;
    float tScaled = e0 * p0t.z + e1 * p1t.z + e2 * p2t.z;
    if (det < 0.0f && (tScaled >= ray.tnear * det || tScaled < ray.tfar * det))
        return false;
    else if (det > 0.0f && (tScaled <= ray.tnear * det || tScaled > ray.tfar * det))
        return false;

    // Compute barycentric coordinates and t value for triangle intersection
    float invDet = 1.0f / det;
    float b0 = e0 * invDet;
    float b1 = e1 * invDet;
    float b2 = e2 * invDet;
    float t = tScaled * invDet;
    assert(b0 + b1 <= 1.0f);
#else
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    constexpr float EPSILON = 0.000001f;

    glm::ivec3 triangle = m_triangles[primitiveID];
    glm::vec3 p0 = m_positions[triangle[0]];
    glm::vec3 p1 = m_positions[triangle[1]];
    glm::vec3 p2 = m_positions[triangle[2]];

    glm::vec3 e1 = p1 - p0;
    glm::vec3 e2 = p2 - p0;
    glm::vec3 h = cross(ray.direction, e2);
    float a = dot(e1, h);
    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - p0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    glm::vec3 q = cross(s, e1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * dot(e2, q);
    if (t < ray.tnear || t > ray.tfar)
        return false;

    float b0 = 1 - u - v;
    float b1 = u;
    float b2 = v;
#endif

    // Compute triangle partial derivatives
    glm::vec3 dpdu, dpdv;
    glm::vec2 uv[3];
    getUVs(primitiveID, uv);
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
    isect = SurfaceInteraction(pHit, uvHit, -ray.direction, dpdu, dpdv, glm::vec3(0.0f), glm::vec3(0.0f), this, primitiveID);

    // Override surface normal in isect for triangle
    isect.normal = isect.shading.normal = glm::normalize(glm::cross(dp02, dp12));

    // Shading normals / tangents
    if (m_normals || m_tangents) {
        glm::ivec3 v = m_triangles[primitiveID];

        // Compute shading normal ns for triangle
        glm::vec3 ns;
        if (m_normals)
            ns = glm::normalize(b0 * m_normals[v[0]] + b1 * m_normals[v[1]] + b2 * m_normals[v[2]]);
        else
            ns = isect.normal;

        // Compute shading tangent ss for triangle
        glm::vec3 ss;
        if (m_tangents)
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
        if (m_normals) {
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
    if (m_normals)
        isect.normal = faceForward(isect.normal, isect.shading.normal);
    //else if (reverseOrientation ^ transformSwapsHandedness)
    //	isect.normal = isect.shading.normal = -isect.normal;

    tHit = t;
    return true;
}
}
