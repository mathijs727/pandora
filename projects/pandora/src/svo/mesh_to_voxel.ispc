#include "ispc/math.h"
#include "ispc/bounds.h"
#include "ispc/voxel_grid.h"

// Helper functions
vec3i worldToVoxel(const vec3 worldVec, const vec3 offset, const vec3 worldToVoxelScale)
{
    vec3i result = ((worldVec - offset) * worldToVoxelScale);
    return result;
}

vec3 voxelToWorld(const vec3i voxel, const vec3 offset, const vec3 voxelToWorldScale)
{
    return voxel * voxelToWorldScale + offset;
}

// Naive mesh voxelization
// Based on: http://research.michael-schwarz.com/publ/files/vox-siga10.pdf
// Outline:
// For each triangle:
//   For each voxel in triangles AABB:
//     Test intersection between voxel and triangle
export void meshToVoxelGrid(uniform unsigned int* uniform voxelGrid, const uniform int resolution, const uniform Bounds& gridBounds, const uniform CPPVec3* uniform positions, uniform const CPPVec3i* uniform triangles, uniform const int N)
{
    // Map world space to [0, 1]
    const uniform float scale = maxComponent(extent(gridBounds));
    const uniform vec3 offset = gridBounds.min;
    const uniform vec3i gridResolution = make_vec3i(resolution);

    // World space extent of a voxel
    const uniform vec3 delta_p = scale / resolution;

    // Arguments to helper functions (no lambdas in ispc :( )
    const uniform vec3 worldToVoxelScale = resolution / scale;
    const uniform vec3 voxelToWorldScale = scale / resolution;

    const uniform vec3i maxGridVoxel = resolution - 1;

    foreach (t = 0 ... N) {
        const vec3i triangle = make_vec3i(triangles[t]);
        const vec3 v[3] = { make_vec3(positions[triangle[0]]), make_vec3(positions[triangle[1]]), make_vec3(positions[triangle[2]]) };
        const vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
        const vec3 n = cross(e[0], e[1]);

        // Triangle bounds
        const vec3 tBoundsMin = min(v[0], min(v[1], v[2]));
        const vec3 tBoundsMax = max(v[0], max(v[1], v[2]));
        const vec3 tBoundsExtent = tBoundsMax - tBoundsMin;

        const vec3i tBoundsMinVoxel = min(worldToVoxel(tBoundsMin, offset, worldToVoxelScale), maxGridVoxel); // Fix for triangles on the border of the voxel grid
        const vec3i tBoundsMaxVoxel = worldToVoxel(tBoundsMin + tBoundsExtent, offset, worldToVoxelScale) + 1; // Upper bound
        const vec3i tBoundsExtentVoxel = tBoundsMaxVoxel - tBoundsMinVoxel;

        // Critical point
        const vec3 c = {
            n.x > 0 ? delta_p.x : 0,
            n.y > 0 ? delta_p.y : 0,
            n.z > 0 ? delta_p.z : 0
		};
        const float d1 = dot(n, c - v[0]);
        const float d2 = dot(n, (delta_p - c) - v[0]);

        // For each voxel in the triangles AABB
        for (int z = tBoundsMinVoxel.z; z < min(tBoundsMaxVoxel.z, gridResolution.z); z++) {
            for (int y = tBoundsMinVoxel.y; y < min(tBoundsMaxVoxel.y, gridResolution.y); y++) {
                for (int x = tBoundsMinVoxel.x; x < min(tBoundsMaxVoxel.x, gridResolution.x); x++) {
                    // Intersection test
                    const vec3 p = voxelToWorld(make_vec3i(x, y, z), offset, voxelToWorldScale);

                    bool planeIntersect = ((dot(n, p) + d1) * (dot(n, p) + d2)) <= 0;
                    if (!planeIntersect)
                        continue;

                    bool triangleIntersect2D = true;
                    for (int i = 0; i < 3; i++) {
                        // Test overlap between the projection of the triangle and AABB on the XY-plane
                        if (abs(n.z) > 0) {
                            const vec2 n_xy_ei = make_vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
							const vec2 v_xy_i = { v[i].x, v[i].y };
							const vec2 p_xy_i = { p.x, p.y };
                            float distFromEdge = dot(p_xy_i, n_xy_ei) + max(0.0f, delta_p.x * n_xy_ei.x) + max(0.0f, delta_p.y * n_xy_ei.y) - dot(n_xy_ei, v_xy_i);
                            triangleIntersect2D &= distFromEdge >= 0;
                        }

                        // Test overlap between the projection of the triangle and AABB on the ZX-plane
                        if (abs(n.y) > 0) {
                            const vec2 n_xz_ei = make_vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
							const vec2 v_xz_i = { v[i].x, v[i].z };
							const vec2 p_xz_i = { p.x, p.z };
                            float distFromEdge = dot(p_xz_i, n_xz_ei) + max(0.0f, delta_p.x * n_xz_ei.x) + max(0.0f, delta_p.z * n_xz_ei.y) - dot(n_xz_ei, v_xz_i);
                            triangleIntersect2D &= distFromEdge >= 0;
                        }

                        // Test overlap between the projection of the triangle and AABB on the YZ-plane
                        if (abs(n.x) > 0) {
                            const vec2 n_yz_ei = make_vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
							const vec2 v_yz_i = { v[i].y, v[i].z };
							const vec2 p_yz_i = { p.y, p.z };
                            float distFromEdge = dot(p_yz_i, n_yz_ei) + max(0.0f, delta_p.y * n_yz_ei.x) + max(0.0f, delta_p.z * n_yz_ei.y) - dot(n_yz_ei, v_yz_i);
                            triangleIntersect2D &= distFromEdge >= 0;
                        }
                    }

					// Test intersection between triangle & voxel bounding boxes
					{
						Bounds voxelBounds;
						voxelBounds.min = p;
						voxelBounds.max = p + delta_p;

						const vec3 overlapBoundsMin = max(tBoundsMin, voxelBounds.min);
						const vec3 overlapBoundsMax = min(tBoundsMax, voxelBounds.max);
						const vec3 overlapBoundsExtent = overlapBoundsMax - overlapBoundsMin;
						triangleIntersect2D &= (overlapBoundsExtent.x >= 0 && overlapBoundsExtent.y >= 0 && overlapBoundsExtent.z >= 0);
					}

					if (triangleIntersect2D)
					{
						int bitPosition = voxelGrid_bitPosition(make_vec3i(x, y, z), resolution);
						foreach_unique(uniqueBitPosition in bitPosition) {
							voxelGrid_setTrue(voxelGrid, uniqueBitPosition);
						}
					}
                }
            }
        }
    }
}
