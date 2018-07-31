#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/geometry/bounds.h"
#include "pandora/geometry/triangle.h"
#include <iostream>

namespace pandora {

// Naive mesh voxelization
// Based on: http://research.michael-schwarz.com/publ/files/vox-siga10.pdf
// Outline:
// For each triangle:
//   For each voxel in triangles AABB:
//     Test intersection between voxel and triangle
void meshToVoxelGridNaive(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh, int resolution)
{
    float scale = maxComponent(gridBounds.getExtent());
    glm::vec3 offset = gridBounds.min;

    // ====================== Setup phase ======================
    glm::vec3 delta_p = glm::vec3(scale / resolution); // Bounding box extents -> extent of a single voxel

    auto triangles = mesh.getTriangles();
    auto positions = mesh.getPositions();

    size_t N = triangles.size();

    std::vector<glm::vec3> tBoundsMins(N); // Min corner of triangle bounds
    std::vector<glm::vec3> tBoundsExtents(N); // Extent of triangle bounds
    std::vector<glm::vec3> ns(N); // Normals

    // Used for 3D AABB / plane overlap test
    std::vector<float> d1s(N);
    std::vector<float> d2s(N);

    // Used for 2D triangle / AABB overlap test
    std::vector<glm::vec2> n_xy_es[3] = { std::vector<glm::vec2>(N), std::vector<glm::vec2>(N), std::vector<glm::vec2>(N) };
    std::vector<float> d_xy_es[3] = { std::vector<float>(N), std::vector<float>(N), std::vector<float>(N) };
    std::vector<glm::vec2> n_zx_es[3] = { std::vector<glm::vec2>(N), std::vector<glm::vec2>(N), std::vector<glm::vec2>(N) };
    std::vector<float> d_zx_es[3] = { std::vector<float>(N), std::vector<float>(N), std::vector<float>(N) };
    std::vector<glm::vec2> n_yz_es[3] = { std::vector<glm::vec2>(N), std::vector<glm::vec2>(N), std::vector<glm::vec2>(N) };
    std::vector<float> d_yz_es[3] = { std::vector<float>(N), std::vector<float>(N), std::vector<float>(N) };

    for (int t = 0; t < triangles.size(); t++) {
        const auto& triangle = triangles[t];
        glm::vec3 v[3] = { positions[triangle[0]], positions[triangle[1]], positions[triangle[2]] };
        glm::vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };

        // Triangle bounds
        glm::vec3 tBoundsMin = glm::min(v[0], glm::min(v[1], v[2]));
        glm::vec3 tBoundsMax = glm::max(v[0], glm::max(v[1], v[2]));
        tBoundsMins[t] = tBoundsMin;
        tBoundsExtents[t] = tBoundsMax - tBoundsMin;

        // Normal
        glm::vec3 n = glm::normalize(glm::cross(e[0], e[1]));
        ns[t] = n;

        // Critical point
        glm::vec3 c(
            n.x > 0 ? delta_p.x : 0,
            n.y > 0 ? delta_p.y : 0,
            n.z > 0 ? delta_p.z : 0);
        d1s[t] = glm::dot(n, c - v[0]);
        d2s[t] = glm::dot(n, (delta_p - c) - v[0]);

        for (int i = 0; i < 3; i++) { // For each edge
            {
                glm::vec2 n_xy_ei = glm::vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
                glm::vec2 v_xy_i(v[i].x, v[i].y);
                float d_xy_ei = -glm::dot(n_xy_ei, v_xy_i) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y);

                n_xy_es[i][t] = n_xy_ei;
                d_xy_es[i][t] = d_xy_ei;
            }

            {
                glm::vec2 n_zx_ei = glm::vec2(-e[i].x, e[i].z) * (n.y >= 0 ? 1.0f : -1.0f);
                glm::vec2 v_zx_i(v[i].z, v[i].x);
                float d_zx_ei = -glm::dot(n_zx_ei, v_zx_i) + std::max(0.0f, delta_p.z * n_zx_ei.x) + std::max(0.0f, delta_p.x * n_zx_ei.y);

                n_zx_es[i][t] = n_zx_ei;
                d_zx_es[i][t] = d_zx_ei;
            }

            {
                glm::vec2 n_yz_ei = glm::vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
                glm::vec2 v_yz_i(v[i].y, v[i].z);
                float d_yz_ei = -glm::dot(n_yz_ei, v_yz_i) + std::max(0.0f, delta_p.y * n_yz_ei.x) + std::max(0.0f, delta_p.z * n_yz_ei.y);

                n_yz_es[i][t] = n_yz_ei;
                d_yz_es[i][t] = d_yz_ei;
            }
        }
    }

    // ==================== Intersect phase ====================
    const glm::vec3 worldToVoxelScale = glm::vec3(resolution) / scale;
    const glm::vec3 voxelToWorldScale = scale / glm::vec3(resolution);
    const auto worldToVoxel = [&](const glm::vec3& worldVec) -> glm::ivec3 { return glm::ivec3((worldVec - offset) * worldToVoxelScale); };
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * voxelToWorldScale + offset; };

    const glm::ivec3 maxGridVoxel(resolution - 1);

	//for (int t = 0; t < triangles.size(); t++) {
	for (int t = 2; t < 3; t++) {
        glm::vec3 tBoundsMin = tBoundsMins[t];
        glm::vec3 tBoundsExtent = tBoundsExtents[t];

        glm::ivec3 tBoundsMinVoxel = glm::min(worldToVoxel(tBoundsMin), maxGridVoxel);// Fix for triangles on the border of the voxel grid
        glm::ivec3 tBoundsMaxVoxel = worldToVoxel(tBoundsMin + tBoundsExtent); // Upper bound
		
		tBoundsMinVoxel = glm::ivec3(0);
		tBoundsMaxVoxel = maxGridVoxel; // Upper bound

        // For each voxel in the triangles AABB
        for (int z = tBoundsMinVoxel.z; z <= std::min(tBoundsMaxVoxel.z, maxGridVoxel.z); z++) {
            for (int y = tBoundsMinVoxel.y; y <= std::min(tBoundsMaxVoxel.y, maxGridVoxel.y); y++) {
                for (int x = tBoundsMinVoxel.x; x <= std::min(tBoundsMaxVoxel.x, maxGridVoxel.x); x++) {
                    // Intersection test
                    glm::vec3 p = voxelToWorld(glm::ivec3(x, y, z));

                    bool planeIntersect = ((glm::dot(ns[t], p) + d1s[t]) * (glm::dot(ns[t], p) + d2s[t])) <= 0;
                    if (!planeIntersect)
                        continue;

					bool triangleIntersect2D = true;
                    for (int i = 0; i < 3; i++) {
                        triangleIntersect2D &= (glm::dot(n_xy_es[i][t], glm::vec2(p.x, p.y)) + d_xy_es[i][t]) >= 0;
						triangleIntersect2D &= (glm::dot(n_zx_es[i][t], glm::vec2(p.z, p.x)) + d_zx_es[i][t]) >= 0;
                        triangleIntersect2D &= (glm::dot(n_yz_es[i][t], glm::vec2(p.y, p.z)) + d_yz_es[i][t]) >= 0;

						// Equivalent in-line version
						/*const auto& triangle = triangles[t];
						glm::vec3 v[3] = { positions[triangle[0]], positions[triangle[1]], positions[triangle[2]] };
						glm::vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
						glm::vec3 n = glm::cross(e[0], e[1]);
						{
							glm::vec2 n_xy_ei = glm::vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
							glm::vec2 v_xy_i(v[i].x, v[i].y);
							glm::vec2 p_xy_i(p.x, p.y);
							float distFromEdge = glm::dot(p_xy_i, n_xy_ei) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y) - glm::dot(n_xy_ei, v_xy_i);
							triangleIntersect2D &= distFromEdge >= 0;
						}

						{
							glm::vec2 n_xz_ei = glm::vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
							glm::vec2 v_xz_i(v[i].x, v[i].z);
							glm::vec2 p_xz_i(p.x, p.z);
							float distFromEdge = glm::dot(p_xz_i, n_xz_ei) + std::max(0.0f, delta_p.x * n_xz_ei.x) + std::max(0.0f, delta_p.z * n_xz_ei.y) - glm::dot(n_xz_ei, v_xz_i);
							triangleIntersect2D &= distFromEdge >= 0;
						}

						{
							glm::vec2 n_yz_ei = glm::vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
							glm::vec2 v_yz_i(v[i].y, v[i].z);
							glm::vec2 p_yz_i(p.y, p.z);
							float distFromEdge = glm::dot(p_yz_i, n_yz_ei) + std::max(0.0f, delta_p.y * n_yz_ei.x) + std::max(0.0f, delta_p.z * n_yz_ei.y) - glm::dot(n_yz_ei, v_yz_i);
							triangleIntersect2D &= distFromEdge >= 0;
						}*/
                    }
					
					Bounds triangleBounds = mesh.getPrimitiveBounds(t);
					Bounds voxelBounds = Bounds(p, p+delta_p);
					triangleIntersect2D &= triangleBounds.overlaps(voxelBounds);
					//if (!triangleBounds.overlaps(voxelBounds))
					//	std::cout << "CULL" << std::endl;

                    if (triangleIntersect2D)
                        voxelGrid.set(x, y, z, true);
                }
            }
        }
    }
}

}
