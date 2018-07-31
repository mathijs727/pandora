#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/geometry/bounds.h"
#include "pandora/geometry/triangle.h"
#include "pandora/svo/voxel_grid.h"
#include <iostream>

namespace pandora {

struct VoxelizeTriangleData {
    // Offset and scale to transform from/to world/voxel coordinates
    glm::vec3 offset;
    glm::vec3 voxelToWorldScale;
    glm::vec3 worldToVoxelScale;

    // World size of a voxel
    glm::vec3 delta_p;

    // The highest voxel (usefull for bounds checks)
    glm::ivec3 maxGridVoxel;

    // Triangle data
    glm::vec3 v[3];
    glm::vec3 e[3];
    glm::vec3 n;

    // Triangle bounds
    glm::vec3 tBoundsMin;
    glm::vec3 tBoundsMax;
    glm::ivec3 tBoundsMinVoxel;
    glm::ivec3 tBoundsMaxVoxel;
};

void fullTest(VoxelGrid& voxelGrid, const VoxelizeTriangleData& data)
{
    const glm::vec3& delta_p = data.delta_p;
    const glm::vec3* v = data.v;
    const glm::vec3* e = data.e;
    const glm::vec3& n = data.n;
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * data.voxelToWorldScale + data.offset; };

    // Critical point
    glm::vec3 c(
        n.x > 0 ? delta_p.x : 0,
        n.y > 0 ? delta_p.y : 0,
        n.z > 0 ? delta_p.z : 0);
    float d1 = glm::dot(n, c - v[0]);
    float d2 = glm::dot(n, (delta_p - c) - v[0]);

    // For each voxel in the triangles AABB
    for (int z = data.tBoundsMinVoxel.z; z <= std::min(data.tBoundsMaxVoxel.z, data.maxGridVoxel.z); z++) {
        for (int y = data.tBoundsMinVoxel.y; y <= std::min(data.tBoundsMaxVoxel.y, data.maxGridVoxel.y); y++) {
            for (int x = data.tBoundsMinVoxel.x; x <= std::min(data.tBoundsMaxVoxel.x, data.maxGridVoxel.x); x++) {
                // Intersection test
                glm::vec3 p = voxelToWorld(glm::ivec3(x, y, z));

                bool planeIntersect = ((glm::dot(n, p) + d1) * (glm::dot(n, p) + d2)) <= 0;
                if (!planeIntersect)
                    continue;

                bool triangleIntersect2D = true;
                for (int i = 0; i < 3; i++) {
                    // Test overlap between the projection of the triangle and AABB on the XY-plane
                    {
                        glm::vec2 n_xy_ei = glm::vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
                        glm::vec2 v_xy_i(v[i].x, v[i].y);
                        glm::vec2 p_xy_i(p.x, p.y);
                        float distFromEdge = glm::dot(p_xy_i, n_xy_ei) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y) - glm::dot(n_xy_ei, v_xy_i);
                        triangleIntersect2D &= distFromEdge >= 0;
                    }

                    // Test overlap between the projection of the triangle and AABB on the ZX-plane
                    {
                        glm::vec2 n_xz_ei = glm::vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
                        glm::vec2 v_xz_i(v[i].x, v[i].z);
                        glm::vec2 p_xz_i(p.x, p.z);
                        float distFromEdge = glm::dot(p_xz_i, n_xz_ei) + std::max(0.0f, delta_p.x * n_xz_ei.x) + std::max(0.0f, delta_p.z * n_xz_ei.y) - glm::dot(n_xz_ei, v_xz_i);
                        triangleIntersect2D &= distFromEdge >= 0;
                    }

                    // Test overlap between the projection of the triangle and AABB on the YZ-plane
                    {
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

// Naive mesh voxelization
// Based on: http://research.michael-schwarz.com/publ/files/vox-siga10.pdf
// Outline:
// For each triangle:
//   For each voxel in triangles AABB:
//     Test intersection between voxel and triangle
void meshToVoxelGridNaive(VoxelGrid& voxelGrid, const Bounds& gridBounds, const TriangleMesh& mesh)
{
    VoxelizeTriangleData data;

    // Map world space to [0, 1]
    float scale = maxComponent(gridBounds.extent());
    data.offset = gridBounds.min;
	data.maxGridVoxel = glm::ivec3(voxelGrid.resolution() - 1);

    // World space extent of a voxel
    data.delta_p = glm::vec3(scale / voxelGrid.resolution());

    // Helper functions
    data.worldToVoxelScale = glm::vec3(voxelGrid.resolution()) / scale;
    data.voxelToWorldScale = scale / glm::vec3(voxelGrid.resolution());
    const auto worldToVoxel = [&](const glm::vec3& worldVec) -> glm::ivec3 { return glm::ivec3((worldVec - data.offset) * data.worldToVoxelScale); };
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * data.voxelToWorldScale + data.offset; };

    const glm::ivec3 maxGridVoxel(voxelGrid.resolution() - 1);

    auto triangles = mesh.getTriangles();
    auto positions = mesh.getPositions();
    for (int t = 0; t < triangles.size(); t++) {
        const auto& triangle = triangles[t];
		data.v[0] = positions[triangle[0]];
		data.v[1] = positions[triangle[1]];
		data.v[2] = positions[triangle[2]];

		data.e[0] = data.v[1] - data.v[0];
		data.e[1] = data.v[2] - data.v[1];
		data.e[2] = data.v[0] - data.v[2];
        data.n = glm::cross(data.e[0], data.e[1]);

        // Triangle bounds
        data.tBoundsMin = glm::min(data.v[0], glm::min(data.v[1], data.v[2]));
        data.tBoundsMax = glm::max(data.v[0], glm::max(data.v[1], data.v[2]));
        glm::vec3 tBoundsExtent = data.tBoundsMax - data.tBoundsMin;
        data.tBoundsMinVoxel = glm::min(worldToVoxel(data.tBoundsMin), data.maxGridVoxel); // Fix for triangles on the border of the voxel grid
        data.tBoundsMaxVoxel = worldToVoxel(data.tBoundsMin + tBoundsExtent); // Upper bound

        glm::ivec3 tBoundsExtentVoxel = data.tBoundsMaxVoxel - data.tBoundsMinVoxel;
        /*if (tBoundsExtentVoxel.x == 1 && tBoundsExtentVoxel.y == 1 && tBoundsExtentVoxel.z == 1) {
            voxelGrid.set(data.tBoundsMin.x, data.tBoundsMin.y, data.tBoundsMin.z, true);
        } else {
            fullTest(voxelGrid, data);
        }*/
		fullTest(voxelGrid, data);
    }
}

}
