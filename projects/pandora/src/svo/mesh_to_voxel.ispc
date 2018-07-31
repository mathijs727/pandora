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

vec2 make_vec2(float x, float y)
{
	vec2 result = { x, y };
	return result;
}

vec3 make_vec3(float x, float y, float z)
{
	vec3 result = { x, y, z };
	return result;
}

ivec3 make_ivec3(int x, int y, int z)
{
	ivec3 result = { x, y, z };
	return result;
}

ivec3 make_ivec3(int v)
{
	ivec3 result = { v, v, v };
	return result;
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
    const vec3 offset = gridBounds.min;
    const ivec3 gridResolution = make_ivec3(resolution);

    // World space extent of a voxel
    const vec3 delta_p = scale / resolution;

    // Arguments to helper functions (no lambdas in ispc :( )
    const vec3 worldToVoxelScale = resolution / scale;
    const vec3 voxelToWorldScale = scale / resolution;

    const ivec3 maxGridVoxel = resolution - 1;

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
				voxelGrid[uniqueIdx] = 1;
			}
        } else {
            // Critical point
            const vec3 c = {
                n.x > 0 ? delta_p.x : 0,
                n.y > 0 ? delta_p.y : 0,
                n.z > 0 ? delta_p.z : 0
			};
            const float d1 = dot(n, c - v[0]);
            const float d2 = dot(n, (delta_p - c) - v[0]);

            // For each voxel in the triangles AABB
			//print("GRID RES: (%,%,%)", gridResolution.x, gridResolution.y, gridResolution.z);
            for (int z = tBoundsMinVoxel.z; z < min(tBoundsMaxVoxel.z, gridResolution.z); z++) {
                for (int y = tBoundsMinVoxel.y; y < min(tBoundsMaxVoxel.y, gridResolution.y); y++) {
                    for (int x = tBoundsMinVoxel.x; x < min(tBoundsMaxVoxel.x, gridResolution.x); x++) {
                        // Intersection test
                        const vec3 p = voxelToWorld(make_ivec3(x, y, z), offset, voxelToWorldScale);

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
                            voxelGrid[index(make_ivec3(x, y, z), resolution)] = 1;
                    }
                }
            }
        }
    }
}
