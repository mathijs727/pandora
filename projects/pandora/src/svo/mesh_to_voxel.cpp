#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/geometry/bounds.h"
#include "pandora/geometry/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include <iostream>

#ifdef PANDORA_ISPC_SUPPORT
#include "mesh_to_voxel_ispc.h"
#endif

namespace pandora {

static void meshToVoxelGridScalar(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh);

void meshToVoxelGrid(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh)
{
#ifdef PANDORA_ISPC_SUPPORT
	ispc::Bounds ispcGridBounds;
	ispcGridBounds.min.v[0] = gridBounds.min.x;
	ispcGridBounds.min.v[1] = gridBounds.min.y;
	ispcGridBounds.min.v[2] = gridBounds.min.z;

	ispcGridBounds.max.v[0] = gridBounds.max.x;
	ispcGridBounds.max.v[1] = gridBounds.max.y;
	ispcGridBounds.max.v[2] = gridBounds.max.z;

	const auto& triangles = mesh.getTriangles();
	const auto& positions = mesh.getPositions();

	const ispc::CPPVec3* ispcPositions = reinterpret_cast<const ispc::CPPVec3*>(positions.data());
	const ispc::CPPVec3i* ispcTriangles = reinterpret_cast<const ispc::CPPVec3i*>(triangles.data());

	ispc::meshToVoxelGrid(voxelGrid.data(), voxelGrid.resolution(), ispcGridBounds, ispcPositions, ispcTriangles, (uint32_t)triangles.size());
#else
	meshToVoxelGridScalar(voxelGrid, gridBounds, mesh);
#endif

}

static inline int index(const glm::ivec3 v, int resolution)
{
	assert(v.x >= 0 && v.x < resolution);
	assert(v.y >= 0 && v.y < resolution);
	assert(v.z >= 0 && v.z < resolution);
	return v.z * resolution * resolution + v.y * resolution + v.x;
}

// Naive mesh voxelization
// Based on: http://research.michael-schwarz.com/publ/files/vox-siga10.pdf
// Outline:
// For each triangle:
//   For each voxel in triangles AABB:
//     Test intersection between voxel and triangle
static void meshToVoxelGridScalar(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh)
{
    // Map world space to [0, 1]
    float scale = maxComponent(gridBounds.extent());
    glm::vec3 offset = gridBounds.min;
    glm::ivec3 gridResolution = glm::ivec3(voxelGrid.resolution());

    // World space extent of a voxel
    glm::vec3 delta_p = scale / glm::vec3(voxelGrid.resolution());

    // Helper functions
    glm::vec3 worldToVoxelScale = glm::vec3(voxelGrid.resolution()) / scale;
    glm::vec3 voxelToWorldScale = scale / glm::vec3(voxelGrid.resolution());
    const auto worldToVoxel = [&](const glm::vec3& worldVec) -> glm::ivec3 { return glm::ivec3((worldVec - offset) * worldToVoxelScale); };
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * voxelToWorldScale + offset; };

    const glm::ivec3 maxGridVoxel(voxelGrid.resolution() - 1);

    auto triangles = mesh.getTriangles();
    auto positions = mesh.getPositions();
    for (int t = 0; t < triangles.size(); t++) {
        auto triangle = triangles[t];
        glm::vec3 v[3] = { positions[triangle[0]], positions[triangle[1]], positions[triangle[2]] };
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
            voxelGrid.set(tBoundsMinVoxel.x, tBoundsMinVoxel.y, tBoundsMinVoxel.z, true);
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
                            voxelGrid.set(x, y, z, true);
                    }
                }
            }
        }
    }
}

}
