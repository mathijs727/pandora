#include "ispc/core.h"
#include "ispc/math.h"

typedef uint32_t ChildDescriptor;

inline bool ChildDescriptor_isValid(const ChildDescriptor descriptor, const int i)
{
	uint32_t validMask = (descriptor >> 8) & 0xFF;
	return validMask & (1 << i);
}

inline bool ChildDescriptor_isLeaf(const ChildDescriptor descriptor, int i)
{
	uint32_t validMask = (descriptor >> 8) & 0xFF;
	uint32_t leafMask = descriptor & 0xFF;
	return (validMask & leafMask) & (1 << i);
}

struct SparseVoxelOctree
{
	const uniform ChildDescriptor* descriptors;
	const uniform ChildDescriptor rootNode;
};

inline ChildDescriptor SparseVoxelOctree_getChild(const uniform SparseVoxelOctree& svo, const ChildDescriptor descriptor, const int idx)
{
	// TODO: more efficient implementation using popcount (algorithm based, there is no SIMD popcnt)
	uint32_t activeChildCount = 0;
	for (int i = 0; i < idx; i++) {
		if (ChildDescriptor_isValid(descriptor, i) && !ChildDescriptor_isLeaf(descriptor, i)) {
			activeChildCount++;
		}
	}
	uint32_t childPtr = descriptor >> 16;
	return svo.descriptors[childPtr + activeChildCount];
}

struct HitSOA
{
	uniform uint8_t* hit;
	uniform float* t;
};

export void SparseVoxelOctree_intersect(const uniform SparseVoxelOctree& svo, const uniform RaySOA& rays, uniform HitSOA& outHits, const uniform int N)
{
    foreach (i = 0 ... N) {
        // Based on the reference implementation of Efficient Sparse Voxel Octrees:
        // https://github.com/poelzi/efficient-sparse-voxel-octrees/blob/master/src/octree/cuda/Raycast.inl
        const int CAST_STACK_DEPTH = 23;

        // Get rid of small ray direction components to avoid division by zero
        const float epsilon = 1.1920928955078125e-07f; //exp2f(-CAST_STACK_DEPTH);

        vec3 rayOrigin = { rays.originX[i], rays.originY[i], rays.originZ[i] };
        vec3 rayDirection = { rays.directionX[i], rays.directionY[i], rays.directionZ[i] };

        if (abs(rayDirection.x) < epsilon)
            rayDirection.x = copysign(epsilon, rayDirection.x);
        if (abs(rayDirection.y) < epsilon)
            rayDirection.y = copysign(epsilon, rayDirection.y);
        if (abs(rayDirection.z) < epsilon)
            rayDirection.z = copysign(epsilon, rayDirection.z);

        // Precompute the coefficients of tx(x), ty(y) and tz(z).
        // The octree is assumed to reside at coordinates [1, 2].
        vec3 tCoef = 1.0f / -abs(rayDirection);
        vec3 tBias = tCoef * rayOrigin;

        // Select octant mask to mirror the coordinate system so taht ray direction is negative along each axis
        uint32_t octantMask = 7;
        if (rayDirection.x > 0.0f) {
            octantMask ^= (1 << 0);
            tBias.x = 3.0f * tCoef.x - tBias.x;
        }
        if (rayDirection.y > 0.0f) {
            octantMask ^= (1 << 1);
            tBias.y = 3.0f * tCoef.y - tBias.y;
        }
        if (rayDirection.z > 0.0f) {
            octantMask ^= (1 << 2);
            tBias.z = 3.0f * tCoef.z - tBias.z;
        }

        // Initialize the current voxel to the first child of the root
        ChildDescriptor parent = svo.rootNode;
        int idx = 0;
        vec3 pos = make_vec3(1.0f);
        int scale = CAST_STACK_DEPTH - 1;
        float scaleExp2 = 0.5f; // exp2f(scale - sMax)

        // Initialize the active span of t-values
        float tMin = maxComponent(2.0f * tCoef - tBias);
        float tMax = minComponent(tCoef - tBias);
        tMin = max(tMin, 0.0f);

        // Intersection of ray (negative in all directions) with the root node (cube at [1, 2])
        if (1.5 * tCoef.x - tBias.x > tMin) {
            idx ^= (1 << 0);
            pos.x = 1.5f;
        }
        if (1.5 * tCoef.y - tBias.y > tMin) {
            idx ^= (1 << 1);
            pos.y = 1.5f;
        }
        if (1.5 * tCoef.z - tBias.z > tMin) {
            idx ^= (1 << 2);
            pos.z = 1.5f;
        }

		// Should be of size CAST_STACK_DEPTH + 1, but a "const int" is not a compile-time constant according to the ISPC compiler (and no constexpr available)
		varying ChildDescriptor stackParent[24];
		varying float stackTmax[24];
        while (scale < CAST_STACK_DEPTH) {
            // === INTERSECT ===
            // Determine the maximum t-value of the cube by evaluating tx(), ty() and tz() at its corner
            vec3 tCorner = pos * tCoef - tBias;
            float tcMax = minComponent(tCorner);

            // Process voxel if the corresponding bit in the valid mask is set
            int childIndex = 7 - idx ^ octantMask; // TODO: this might need to be 7 - childIndex
            if (ChildDescriptor_isValid(parent, childIndex) && tMin <= tMax) {
                // === INTERSECT ===
                float tvMax = min(tMax, tcMax);
                float half = scaleExp2 * 0.5f;
                vec3 tCenter = half * tCoef + tCorner;

                if (ChildDescriptor_isLeaf(parent, childIndex)) {
                    break; // Line 231
                }

                if (tMin <= tvMax) {
                    // === PUSH ===
                    //stack[scale] = { parent, tMax };
					stackParent[scale] = parent;
					stackTmax[scale] = tMax;

                    // Find child descriptor corresponding to the current voxel
                    parent = SparseVoxelOctree_getChild(svo, parent, childIndex);

                    // Select the child voxel that the ray enters first.
                    idx = 0;
                    scale--;
                    scaleExp2 = half;
                    if (tCenter.x > tMin) {
                        idx ^= (1 << 0);
                        pos.x += scaleExp2;
                    }
                    if (tCenter.y > tMin) {
                        idx ^= (1 << 1);
                        pos.y += scaleExp2;
                    }
                    if (tCenter.z > tMin) {
                        idx ^= (1 << 2);
                        pos.z += scaleExp2;
                    }

                    // Update active t-span
                    tMax = tvMax;

                    continue;
                }
            }

            // === ADVANCE ===

            // Step along the ray
            int stepMask = 0;
            if (tCorner.x <= tcMax) {
                stepMask ^= (1 << 0);
                pos.x -= scaleExp2;
            }
            if (tCorner.y <= tcMax) {
                stepMask ^= (1 << 1);
                pos.y -= scaleExp2;
            }
            if (tCorner.z <= tcMax) {
                stepMask ^= (1 << 2);
                pos.z -= scaleExp2;
            }

            // Update active t-span and flip bits of the child slot index
            tMin = tcMax;
            idx ^= stepMask;

            // Proceed with pop if the bit flip disagree with the ray direction
            if ((idx & stepMask) != 0) {
                // === POP ===
                // Find the highest differing bit between the two positions
                int differingBits = 0;
                if ((stepMask & (1 << 0)) != 0) {
                    differingBits |= floatAsInt(pos.x) ^ floatAsInt(pos.x + scaleExp2);
                }
                if ((stepMask & (1 << 1)) != 0) {
                    differingBits |= floatAsInt(pos.y) ^ floatAsInt(pos.y + scaleExp2);
                }
                if ((stepMask & (1 << 2)) != 0) {
                    differingBits |= floatAsInt(pos.z) ^ floatAsInt(pos.z + scaleExp2);
                }
                scale = (floatAsInt((float)differingBits) >> 23) - 127; // Position of the highest bit (complicated alternative to bitscan)
                scaleExp2 = intAsFloat((scale - CAST_STACK_DEPTH + 127) << 23); // exp2f(scale - s_max)


                // Restore parent voxel from the stack
                parent = stackParent[scale];
                tMax = stackTmax[scale];

                // Round cube position and extract child slot index
                int shx = floatAsInt(pos.x) >> scale;
                int shy = floatAsInt(pos.y) >> scale;
                int shz = floatAsInt(pos.z) >> scale;
                pos.x = intAsFloat(shx << scale);
                pos.y = intAsFloat(shy << scale);
                pos.z = intAsFloat(shz << scale);
                idx = (shx & 1) | ((shy & 1) << 1) | ((shz & 1) << 2);
            }
        }

		outHits.hit[i] = (scale < CAST_STACK_DEPTH);
		outHits.t[i] = tMin;

    }
}
