typedef float<2> vec2;
typedef float<3> vec3;
typedef int<3> ivec3;

float dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec3 min(const vec3 a, const vec3 b)
{
    float r0 = min(a.x, b.x);
    float r1 = min(a.y, b.y);
    float r2 = min(a.z, b.z);
    vec3 result = { r0, r1, r2 };
    return result;
}

vec3 max(const vec3 a, const vec3 b)
{
	float r0 = max(a.x, b.x);
	float r1 = max(a.y, b.y);
	float r2 = max(a.z, b.z);
	vec3 result = { r0, r1, r2 };
	return result;
}

vec3 cross(vec3 u, vec3 v)
{
    // https://en.wikipedia.org/wiki/Cross_product
    float r0 = u[1] * v[2] - u[2] * v[1];
    float r1 = u[2] * v[0] - u[0] * v[2];
    float r2 = u[0] * v[1] - u[1] * v[0];

    vec3 result = { r0, r1, r2 };
    return result;
}

struct Bounds {
    vec3 min;
    vec3 max;
};

vec3 extent(const Bounds bounds)
{
    return bounds.max - bounds.min;
}

inline float maxComponent(const vec3 v)
{
    return max(v.x, max(v.y, v.z));
}

// Helper functions
ivec3 worldToVoxel(const vec3 worldVec, const vec3 offset, const vec3 worldToVoxelScale)
{
    ivec3 result = ((worldVec - offset) * worldToVoxelScale);
    return result;
}

vec3 voxelToWorld(const ivec3 voxel, const vec3 offset, const vec3 voxelToWorldScale)
{
    return voxel * voxelToWorldScale + offset;
}

int index(const ivec3 v, int resolution)
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
export void meshToVoxelGrid(uniform int8* uniform voxelGrid, const uniform int resolution, const uniform Bounds& gridBounds, const uniform vec3* uniform positions, uniform const ivec3* uniform triangles, uniform const int N)
{
    // Map world space to [0, 1]
    float scale = maxComponent(extent(gridBounds));
    vec3 offset = gridBounds.min;
    ivec3 gridResolution = { resolution };

    // World space extent of a voxel
    vec3 delta_p = scale / resolution;

    // Arguments to helper functions (no lambdas in ispc :( )
    vec3 worldToVoxelScale = resolution / scale;
    vec3 voxelToWorldScale = scale / resolution;

    const ivec3 maxGridVoxel = resolution - 1;

	/*// Sphere
	uniform ivec3 center = resolution / 2;
	uniform float radius2 = (resolution / 2) * (resolution / 2);
	foreach_tiled(x = 0 ... resolution, y = 0 ... resolution, z = 0 ... resolution)
	{
		ivec3 p = { x, y, z };
		ivec3 d = p - center;
		float dist2 = d.x * d.x + d.y*d.y + d.z * d.z;
		if (dist2 < radius2)
		{
			foreach_active(i) {
				voxelGrid[index(p, resolution)] = true;
			}
		}
	}

	return;*/


    foreach (t = 0 ... N) {
        const ivec3 triangle = triangles[t];
        const vec3 v[3] = { positions[triangle[0]], positions[triangle[1]], positions[triangle[2]] };
        const vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
        const vec3 n = cross(e[0], e[1]);

        // Triangle bounds
        const vec3 tBoundsMin = min(v[0], min(v[1], v[2]));
        const vec3 tBoundsMax = max(v[0], max(v[1], v[2]));
        const vec3 tBoundsExtent = tBoundsMax - tBoundsMin;

        const ivec3 tBoundsMinVoxel = min(worldToVoxel(tBoundsMin, offset, worldToVoxelScale), maxGridVoxel); // Fix for triangles on the border of the voxel grid
        const ivec3 tBoundsMaxVoxel = worldToVoxel(tBoundsMin + tBoundsExtent, offset, worldToVoxelScale) + 1; // Upper bound
        const ivec3 tBoundsExtentVoxel = tBoundsMaxVoxel - tBoundsMinVoxel;

        if (tBoundsExtentVoxel.x == 1 && tBoundsExtentVoxel.y == 1 && tBoundsExtentVoxel.z == 1) {
			int idx = index(tBoundsMinVoxel, resolution);
			foreach_unique(uniqueIdx in idx) {
				voxelGrid[uniqueIdx] = true;
			}
        }
		
		/* else {
            // Critical point
            vec3 c(
                n.x > 0 ? delta_p.x : 0,
                n.y > 0 ? delta_p.y : 0,
                n.z > 0 ? delta_p.z : 0);
            float d1 = dot(n, c - v[0]);
            float d2 = dot(n, (delta_p - c) - v[0]);

            // For each voxel in the triangles AABB
            for (int z = tBoundsMinVoxel.z; z < std::min(tBoundsMaxVoxel.z, gridResolution.z); z++) {
                for (int y = tBoundsMinVoxel.y; y < std::min(tBoundsMaxVoxel.y, gridResolution.y); y++) {
                    for (int x = tBoundsMinVoxel.x; x < std::min(tBoundsMaxVoxel.x, gridResolution.x); x++) {
                        // Intersection test
                        vec3 p = voxelToWorld(ivec3(x, y, z));

                        bool planeIntersect = ((dot(n, p) + d1) * (dot(n, p) + d2)) <= 0;
                        if (!planeIntersect)
                            continue;

                        bool triangleIntersect2D = true;
                        for (int i = 0; i < 3; i++) {
                            // Test overlap between the projection of the triangle and AABB on the XY-plane
                            if (std::abs(n.z) > 0) {
                                vec2 n_xy_ei = vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
                                vec2 v_xy_i(v[i].x, v[i].y);
                                vec2 p_xy_i(p.x, p.y);
                                float distFromEdge = dot(p_xy_i, n_xy_ei) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y) - dot(n_xy_ei, v_xy_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the ZX-plane
                            if (std::abs(n.y) > 0) {
                                vec2 n_xz_ei = vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
                                vec2 v_xz_i(v[i].x, v[i].z);
                                vec2 p_xz_i(p.x, p.z);
                                float distFromEdge = dot(p_xz_i, n_xz_ei) + std::max(0.0f, delta_p.x * n_xz_ei.x) + std::max(0.0f, delta_p.z * n_xz_ei.y) - dot(n_xz_ei, v_xz_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the YZ-plane
                            if (std::abs(n.x) > 0) {
                                vec2 n_yz_ei = vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
                                vec2 v_yz_i(v[i].y, v[i].z);
                                vec2 p_yz_i(p.y, p.z);
                                float distFromEdge = dot(p_yz_i, n_yz_ei) + std::max(0.0f, delta_p.y * n_yz_ei.x) + std::max(0.0f, delta_p.z * n_yz_ei.y) - dot(n_yz_ei, v_yz_i);
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
        }*/
    }
}
