#include "pandora/graphics_core/transform.h"
#include "pandora/shapes/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <glm/glm.hpp>
#include <optick/optick.h>

namespace pandora {

size_t TriangleShape::sizeBytes() const
{
    size_t size = sizeof(TriangleShape);
    size += m_indices.size() * sizeof(glm::uvec3);
    size += m_positions.size() * sizeof(glm::vec3);
    size += m_normals.size() * sizeof(glm::vec3);
    size += m_texCoords.size() * sizeof(glm::vec2);
    return size;
}

RTCGeometry TriangleShape::createEmbreeGeometry(RTCDevice embreeDevice) const
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

float TriangleShape::primitiveArea(unsigned primitiveID) const
{
    glm::uvec3 triangle = m_indices[primitiveID];
    const glm::vec3& p0 = m_positions[triangle[0]];
    const glm::vec3& p1 = m_positions[triangle[1]];
    const glm::vec3& p2 = m_positions[triangle[2]];
    return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
}

// PBRTv3 page 839
Interaction TriangleShape::samplePrimitive(unsigned primitiveID, PcgRng& rng) const
{
    // Compute uniformly sampled barycentric coordinates
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    float su0 = std::sqrt(rng.uniformFloat());
    glm::vec2 b = glm::vec2(1 - su0, rng.uniformFloat() * su0);

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
Interaction TriangleShape::samplePrimitive(unsigned primitiveID, const Interaction& ref, PcgRng& rng) const
{
    (void)ref;
    auto it = samplePrimitive(primitiveID, rng);
    auto dir = it.position - ref.position;
    if (glm::dot(it.normal, -dir) < 0.0f)
        it.normal = -it.normal;
    return it;
}

// PBRTv3 page 837
float TriangleShape::pdfPrimitive(unsigned primitiveID, const Interaction& ref) const
{
    (void)ref;
    return 1.0f / primitiveArea(primitiveID);
}

// PBRTv3 page 837
float TriangleShape::pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const
{
    if (glm::dot(ref.normal, wi) <= 0.0f)
        return 0.0f;

    // Intersect sample ray with area light geometry
    Ray ray = ref.spawnRay(wi);
    RayHit hitInfo;
    if (!intersectPrimitive(ray, hitInfo, primitiveID))
        return 0.0f;

    //SurfaceInteraction isectLight = fillSurfaceInteraction(ray, hitInfo, false);
    glm::vec3 position = ray.origin + ray.tfar * ray.direction;

    // Convert light sample weight to solid angle measure
    const float distSquared = distanceSquared(ref.position, position);
    const float cosNormal = absDot(hitInfo.geometricNormal, -wi);
    const float primArea = primitiveArea(primitiveID);
    return distSquared / (cosNormal * primArea);
}

void TriangleShape::voxelize(VoxelGrid& grid, const Transform& transform) const
{
    OPTICK_EVENT();
    ALWAYS_ASSERT(isResident());

    // Map world space to [0, 1]
    float scale = maxComponent(grid.bounds().extent());
    glm::vec3 offset = grid.bounds().min;
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

bool TriangleShape::intersectPrimitive(Ray& ray, RayHit& hitInfo, unsigned primitiveID) const
{
#if 0
    // Based on PBRT v3 triangle intersection test (page 158):
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    //
    // Transform the ray and triangle such that the ray origin is at (0,0,0) and its
    // direction points along the +Z axis. This makes the intersection test easy and
    // allows for watertight intersection testing.
    glm::uvec3 triangle = m_indices[primitiveID];
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

    // Compute the first two barycentric coordinates and t value for triangle intersection
    float invDet = 1.0f / det;
    hitInfo.primitiveID = primitiveID;
    hitInfo.geometricUV = glm::vec2(e0 * invDet, e1 * invDet);
    hitInfo.geometricNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    ray.tfar = tScaled * invDet;
    return true;
#else
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    constexpr float EPSILON = 0.000001f;

    glm::ivec3 triangle = m_indices[primitiveID];
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

    ray.tfar = t;
    hitInfo.primitiveID = primitiveID;
    hitInfo.geometricUV = glm::vec2(u, v);
    hitInfo.geometricNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    if (glm::dot(hitInfo.geometricNormal, -ray.direction) < 0)
        hitInfo.geometricNormal = -hitInfo.geometricNormal;
    return true;
#endif
}
}