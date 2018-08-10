#include "ispc/core.h"
#include "ispc/math.h"

typedef uint32_t RelativeNodeOffset;
typedef uint16_t Descriptor;

inline bool Descriptor_isValid(const Descriptor descriptor, const int i)
{
    uint32_t validMask = (descriptor >> 8) & 0xFF;
    return validMask & (1 << i);
}

inline bool Descriptor_isLeaf(const Descriptor descriptor, int i)
{
    uint32_t validMask = (descriptor >> 8) & 0xFF;
    uint32_t leafMask = descriptor & 0xFF;
    return (validMask & leafMask) & (1 << i);
}

struct SparseVoxelDAG32 {
    const uniform RelativeNodeOffset* descriptors;
    const uniform uint32_t* leafs;
    const uniform uint32_t rootNodeOffset;
};

inline void SparseVoxelDAG32_getChild(const uniform SparseVoxelDAG32& dag, const Descriptor descriptor, uint32_t descriptorOffset, const int idx, Descriptor& outDescriptor, uint32_t& outDescriptorOffset)
{
    uint32_t validMask = descriptor >> 8;
    uint32_t childrenBeforeIdxMask = validMask & ((1 << idx) - 1);
    uint32_t childrenBeforeIdx = popcount8(childrenBeforeIdxMask);

    uint32_t childOffsetOffset = descriptorOffset + 1 + childrenBeforeIdx;
    RelativeNodeOffset relativeChildOffset = dag.descriptors[childOffsetOffset];
    uint32_t absoluteOffset = descriptorOffset - relativeChildOffset;
    outDescriptor = (Descriptor)dag.descriptors[absoluteOffset];
    outDescriptorOffset = absoluteOffset;
}

inline void SparseVoxelDAG32_getLeaf(const uniform SparseVoxelDAG32& dag, const Descriptor descriptor, uint32_t descriptorOffset, const int idx, uint32_t& out1, uint32_t& out2)
{
    uint32_t validMask = descriptor >> 8;
    uint32_t childrenBeforeIdxMask = validMask & ((1 << idx) - 1);
    uint32_t childrenBeforeIdx = popcount8(childrenBeforeIdxMask);

    uint32_t leafOffsetOffset = descriptorOffset + 1 + childrenBeforeIdx;
    uint32_t absoluteLeafOffset = dag.descriptors[leafOffsetOffset];
	out1 = dag.leafs[2 * absoluteLeafOffset];
	out2 = dag.leafs[2 * absoluteLeafOffset + 1];
}

struct HitSOA {
    uniform uint8_t* hit;
    uniform float* t;
};

export void SparseVoxelDAG32_intersect(const uniform SparseVoxelDAG32& svdag, const uniform RaySOA& rays, uniform HitSOA& outHits, const uniform int N)
{
	foreach(i = 0 ... N) {
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
		Descriptor parent = (Descriptor)(*(svdag.descriptors + svdag.rootNodeOffset));
		uint32_t parentOffset = svdag.rootNodeOffset;
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
		Descriptor stackParents[24];
		uint32_t stackParentOffsets[24];

		int depthInLeaf = 0; // 1 if at 2x2x2 level, 2 if at 4x4x4 level, 0 otherwise
		uint32_t leafData1;
		uint32_t leafData2;
		while (scale < CAST_STACK_DEPTH) {
			// === INTERSECT ===
			// Determine the maximum t-value of the cube by evaluating tx(), ty() and tz() at its corner
			vec3 tCorner = pos * tCoef - tBias;
			float tcMax = minComponent(tCorner);

			int bitIndex64 = 63 - ((idx & 0b111111) ^ (octantMask | (octantMask << 3)));
			uint32_t bitMask32 = (1lu << (bitIndex64 & 0b11111));
			int potentialHit1 = leafData1 & bitMask32;
			int potentialHit2 = leafData2 & bitMask32;
			int isUpper = bitIndex64 & 0b100000;
			if (depthInLeaf == 2 && ((isUpper && potentialHit2) || (!isUpper && potentialHit1))) {
				break;
			}

			// Process voxel if the corresponding bit in the valid mask is set
			int childIndex = 7 - ((idx & 0b111) ^ octantMask);
			if (depthInLeaf == 1 || (depthInLeaf == 0 && Descriptor_isValid(parent, childIndex))) {
				float half = scaleExp2 * 0.5f;
				vec3 tCenter = half * tCoef + tCorner;

				// === PUSH ===
				stackParents[scale] = parent;
				stackParentOffsets[scale] = parentOffset;

				if (depthInLeaf == 0) {
					if (Descriptor_isLeaf(parent, childIndex)) {
						SparseVoxelDAG32_getLeaf(svdag, parent, parentOffset, childIndex, leafData1, leafData2);
						depthInLeaf = 1;
						parent = 0;
					} else {
						// Find child descriptor corresponding to the current voxel
						SparseVoxelDAG32_getChild(svdag, parent, parentOffset, childIndex, parent, parentOffset);
					}
				} else {
					depthInLeaf++; // 1 ==> 2
				}

				// Select the child voxel that the ray enters first.
				idx = (idx & 0b111) << 3; // Keep the index in the current level which is required for accessing leaf node bits (which are 2 levels deep conceptually)
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
			} else {
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
					int oldScale = scale;
					scale = (floatAsInt((float)differingBits) >> 23) - 127; // Position of the highest bit (complicated alternative to bitscan)
					scaleExp2 = intAsFloat((scale - CAST_STACK_DEPTH + 127) << 23); // exp2f(scale - s_max)
					depthInLeaf = max(0, depthInLeaf - (scale - oldScale));

					// Restore parent voxel from the stack
					parent = stackParents[scale];
					parentOffset = stackParentOffsets[scale];
				}
#if 1
				// Round cube position and extract child slot index
				int shx = floatAsInt(pos.x) >> scale;
				int shy = floatAsInt(pos.y) >> scale;
				int shz = floatAsInt(pos.z) >> scale;
				pos.x = intAsFloat(shx << scale);
				pos.y = intAsFloat(shy << scale);
				pos.z = intAsFloat(shz << scale);
				//idx = ((shx & 1) << 0) | ((shy & 1) << 1) | ((shz & 1) << 2) | (((shx & 2) >> 1) << 3) | (((shy & 2) >> 1) << 4) | (((shz & 2) >> 1) << 5);
				idx = (shx & 1) | ((shy & 1) << 1) | ((shz & 1) << 2) | ((shx & 2) << 2) | ((shy & 2) << 3) | ((shz & 2) << 4);
#else
				// Alternative implementation with less variable right shifts (which are slow according to the IPSC compiler)
				int shx = floatAsInt(pos.x);
				int shy = floatAsInt(pos.y);
				int shz = floatAsInt(pos.z);
				{
					int mask = 0xFFFFFFFF << scale;
					pos.x = intAsFloat(shx & mask);
					pos.y = intAsFloat(shy & mask);
					pos.z = intAsFloat(shz & mask);
			}
				{
					int mask1 = 0x1 << scale;
					int mask2 = 0x2 << scale;
					idx = ((shx & mask1) | ((shy & mask1) << 1) | ((shz & mask1) << 2) | ((shx & mask2) << 2) | ((shy & mask2) << 3) | ((shz & mask2) << 4)) >> scale;
				}
#endif
		} // Push / pop
	} // While

		outHits.hit[i] = (scale < CAST_STACK_DEPTH);
		outHits.t[i] = tMin;
}
}
