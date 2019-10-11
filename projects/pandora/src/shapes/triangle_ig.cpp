#pragma once
#include "pandora/graphics_core/transform.h"
#include "pandora/shapes/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/math.h"
#include <glm/glm.hpp>

namespace pandora {

TriangleIntersectGeometry::TriangleIntersectGeometry(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions)
    : m_indices(std::move(indices))
    , m_positions(std::move(positions))
{
    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
}

size_t TriangleIntersectGeometry::sizeBytes() const
{
    size_t size = sizeof(TriangleIntersectGeometry);
    size += m_indices.size() * sizeof(glm::uvec3);
    size += m_positions.size() * sizeof(glm::vec3);
    return size;
}

RTCGeometry TriangleIntersectGeometry::createEmbreeGeometry(RTCDevice embreeDevice) const
{

    RTCGeometry embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetSharedGeometryBuffer(
        embreeGeometry,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT3,
        m_indices.data(),
        0,
        sizeof(glm::uvec3),
        m_indices.size());

    rtcSetSharedGeometryBuffer(
        embreeGeometry,
        RTC_BUFFER_TYPE_VERTEX,
        0,
        RTC_FORMAT_FLOAT3,
        m_positions.data(),
        0,
        sizeof(glm::vec3),
        m_positions.size());
    return embreeGeometry;
}

float TriangleIntersectGeometry::primitiveArea(unsigned primitiveID) const
{
    glm::uvec3 triangle = m_indices[primitiveID];
    const glm::vec3& p0 = m_positions[triangle[0]];
    const glm::vec3& p1 = m_positions[triangle[1]];
    const glm::vec3& p2 = m_positions[triangle[2]];
    return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
}

// PBRTv3 page 839
Interaction TriangleIntersectGeometry::samplePrimitive(unsigned primitiveID, const glm::vec2& randomSample) const
{
    // Compute uniformly sampled barycentric coordinates
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    float su0 = std::sqrt(randomSample[0]);
    glm::vec2 b = glm::vec2(1 - su0, randomSample[1] * su0);

    glm::uvec3 triangle = m_indices[primitiveID];
    const glm::vec3& p0 = m_positions[triangle[0]];
    const glm::vec3& p1 = m_positions[triangle[1]];
    const glm::vec3& p2 = m_positions[triangle[2]];

    Interaction it;
    it.position = b[0] * p0 + b[1] * p1 + (1 - b[0] - b[1]) * p2;
    it.normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    return it;
}

// PBRTv3 page 837
Interaction TriangleIntersectGeometry::samplePrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec2& randomSample) const
{
    (void)ref;
    return samplePrimitive(primitiveID, randomSample);
}

// PBRTv3 page 837
float TriangleIntersectGeometry::pdfPrimitive(unsigned primitiveID, const Interaction& ref) const
{
    (void)ref;
    return 1.0f / primitiveArea(primitiveID);
}

/*// PBRTv3 page 837
float TriangleIntersectGeometry::pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const
{
    if (glm::dot(ref.normal, wi) <= 0.0f)
        return 0.0f;

    // Intersect sample ray with area light geometry
    Ray ray = ref.spawnRay(wi);
    RayHit hitInfo;
    if (!intersectPrimitive(ray, hitInfo, primitiveID, false))
        return 0.0f;

    //SurfaceInteraction isectLight = fillSurfaceInteraction(ray, hitInfo, false);
    glm::vec3 position = ray.origin + ray.tfar * ray.direction;

    // Convert light sample weight to solid angle measure
    return distanceSquared(ref.position, position) / (absDot(hitInfo.geometricNormal, -wi) * primitiveArea(primitiveID));
}*/

void TriangleIntersectGeometry::voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const
{
    // Map world space to [0, 1]
    float scale = maxComponent(gridBounds.extent());
    glm::vec3 offset = gridBounds.min;
    glm::ivec3 gridResolution = glm::ivec3(grid.resolution());

    // World space extent of a voxel
    glm::vec3 delta_p = scale / glm::vec3(grid.resolution());

    // Helper functions
    glm::vec3 worldToVoxelScale = glm::vec3(grid.resolution()) / scale;
    glm::vec3 voxelToWorldScale = scale / glm::vec3(grid.resolution());
    const auto worldToVoxel = [&](const glm::vec3& worldVec) -> glm::ivec3 { return glm::ivec3((worldVec - offset) * worldToVoxelScale); };
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * voxelToWorldScale + offset; };

    const glm::ivec3 maxGridVoxel(grid.resolution() - 1);

    for (glm::uvec3 triangle : m_indices) {
        glm::vec3 v[3] = {
            transform.transformPoint(m_positions[triangle[0]]),
            transform.transformPoint(m_positions[triangle[1]]),
            transform.transformPoint(m_positions[triangle[2]])
        };
        glm::vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
        glm::vec3 n = glm::cross(e[0], e[1]);

        // Triangle bounds
        glm::vec3 tBoundsMin = glm::min(v[0], glm::min(v[1], v[2]));
        glm::vec3 tBoundsMax = glm::max(v[0], glm::max(v[1], v[2]));
        glm::vec3 tBoundsExtent = tBoundsMax - tBoundsMin;

        glm::ivec3 tBoundsMinVoxel = glm::min(worldToVoxel(tBoundsMin), gridResolution - 1); // Fix for triangles on the border of the voxel grid
        glm::ivec3 tBoundsMaxVoxel = worldToVoxel(tBoundsMin + tBoundsExtent) + 1; // Upper bound
        glm::ivec3 tBoundsExtentVoxel = tBoundsMaxVoxel - tBoundsMinVoxel;

        if (tBoundsExtentVoxel.x == 1 && tBoundsExtentVoxel.y == 1 && tBoundsExtentVoxel.z == 1) {
            grid.set(tBoundsMinVoxel.x, tBoundsMinVoxel.y, tBoundsMinVoxel.z, true);
        } else {
            // Critical point
            glm::vec3 c(
                n.x > 0 ? delta_p.x : 0,
                n.y > 0 ? delta_p.y : 0,
                n.z > 0 ? delta_p.z : 0);
            float d1 = glm::dot(n, c - v[0]);
            float d2 = glm::dot(n, (delta_p - c) - v[0]);

            // For each voxel in the triangles AABB
            for (int z = tBoundsMinVoxel.z; z < std::min(tBoundsMaxVoxel.z, gridResolution.z); z++) {
                for (int y = tBoundsMinVoxel.y; y < std::min(tBoundsMaxVoxel.y, gridResolution.y); y++) {
                    for (int x = tBoundsMinVoxel.x; x < std::min(tBoundsMaxVoxel.x, gridResolution.x); x++) {
                        // Intersection test
                        glm::vec3 p = voxelToWorld(glm::ivec3(x, y, z));

                        bool planeIntersect = ((glm::dot(n, p) + d1) * (glm::dot(n, p) + d2)) <= 0;
                        if (!planeIntersect)
                            continue;

                        bool triangleIntersect2D = true;
                        for (int i = 0; i < 3; i++) {
                            // Test overlap between the projection of the triangle and AABB on the XY-plane
                            if (std::abs(n.z) > 0) {
                                glm::vec2 n_xy_ei = glm::vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
                                glm::vec2 v_xy_i(v[i].x, v[i].y);
                                glm::vec2 p_xy_i(p.x, p.y);
                                float distFromEdge = glm::dot(p_xy_i, n_xy_ei) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y) - glm::dot(n_xy_ei, v_xy_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the ZX-plane
                            if (std::abs(n.y) > 0) {
                                glm::vec2 n_xz_ei = glm::vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
                                glm::vec2 v_xz_i(v[i].x, v[i].z);
                                glm::vec2 p_xz_i(p.x, p.z);
                                float distFromEdge = glm::dot(p_xz_i, n_xz_ei) + std::max(0.0f, delta_p.x * n_xz_ei.x) + std::max(0.0f, delta_p.z * n_xz_ei.y) - glm::dot(n_xz_ei, v_xz_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the YZ-plane
                            if (std::abs(n.x) > 0) {
                                glm::vec2 n_yz_ei = glm::vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
                                glm::vec2 v_yz_i(v[i].y, v[i].z);
                                glm::vec2 p_yz_i(p.y, p.z);
                                float distFromEdge = glm::dot(p_yz_i, n_yz_ei) + std::max(0.0f, delta_p.y * n_yz_ei.x) + std::max(0.0f, delta_p.z * n_yz_ei.y) - glm::dot(n_yz_ei, v_yz_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }
                        }

                        Bounds triangleBounds;
                        triangleBounds.grow(v[0]);
                        triangleBounds.grow(v[1]);
                        triangleBounds.grow(v[2]);
                        Bounds voxelBounds = Bounds(p, p + delta_p);
                        triangleIntersect2D &= triangleBounds.overlaps(voxelBounds);

                        if (triangleIntersect2D)
                            grid.set(x, y, z, true);
                    }
                }
            }
        }
    }
}

unsigned TriangleIntersectGeometry::numPrimitives() const
{
    return static_cast<unsigned>(m_indices.size());
}

Bounds TriangleIntersectGeometry::computeBounds() const
{
    Bounds bounds;
    for (const glm::vec3& p : m_positions)
        bounds.grow(p);
    return bounds;
}

}