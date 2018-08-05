#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/core/ray.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <EASTL/fixed_vector.h>
#include <cmath>
#include <cstring>
#include <immintrin.h>
#include <libmorton/morton.h>

namespace pandora {

// http://graphics.cs.kuleuven.be/publications/BLD13OCCSVO/BLD13OCCSVO_paper.pdf
SparseVoxelDAG::SparseVoxelDAG(const VoxelGrid& grid)
    : SparseVoxelOctree(grid)
{
    uint32_t rootNodeIndex = copySVO(m_svoRootNode);
	m_dagRootNode = reinterpret_cast<const DAGDescriptor*>(m_allocator.data() + rootNodeIndex);
    std::cout << "Size of SparseVoxelDAG: " << m_allocator.size() * sizeof(decltype(m_allocator)::value_type) << "bytes" << std::endl;
}

uint32_t SparseVoxelDAG::copySVO(SVOChildDescriptor descriptor)
{
    eastl::fixed_vector<uint32_t, 8> children;
    for (int i = 0; i < 8; i++) {
        if (descriptor.isValid(i)) {
            if (descriptor.isLeaf(i)) {
                auto svoDescriptor = SparseVoxelOctree::getChild(descriptor, i);
                DAGDescriptor dagDescriptor(svoDescriptor);
                children.push_back(storeDescriptor(dagDescriptor));
            } else {
                auto child = SparseVoxelOctree::getChild(descriptor, i);
                children.push_back(copySVO(child));
            }
        }
    }
    return storeDescriptor(DAGDescriptor(descriptor), children);
}

const SparseVoxelDAG::DAGDescriptor* SparseVoxelDAG::getChild(const DAGDescriptor* descriptor, int idx) const
{
    uint32_t childMask = (descriptor->validMask ^ descriptor->leafMask) & ((1 << idx) - 1);
    uint32_t activeChildIndex = _mm_popcnt_u64(childMask);

    uint32_t descriptorIndex = (reinterpret_cast<const uint32_t*>(descriptor) - m_allocator.data());
    uint32_t childIndex = m_allocator[descriptorIndex + activeChildIndex + 1];
    return reinterpret_cast<const DAGDescriptor*>(&m_allocator[childIndex]);
}

uint32_t SparseVoxelDAG::storeDescriptor(DAGDescriptor descriptor)
{
    uint32_t baseIndex = static_cast<uint32_t>(m_allocator.size());
    m_allocator.push_back(static_cast<uint32_t>(descriptor));
	return baseIndex;
}

uint32_t SparseVoxelDAG::storeDescriptor(DAGDescriptor descriptor, gsl::span<uint32_t> children)
{
    const uint32_t baseIndex = static_cast<uint32_t>(m_allocator.size());

	m_allocator.push_back(static_cast<uint32_t>(descriptor));
    for (uint32_t child : children) {
		assert(child < m_allocator.size());
        m_allocator.push_back(child);
    }

	return baseIndex;
}

std::optional<float> SparseVoxelDAG::intersectScalar(Ray ray) const
{
    // Based on the reference implementation of Efficient Sparse Voxel Octrees:
    // https://github.com/poelzi/efficient-sparse-voxel-octrees/blob/master/src/octree/cuda/Raycast.inl
    constexpr int CAST_STACK_DEPTH = 23; //intLog2(m_resolution);

    // Get rid of small ray direction components to avoid division by zero
    constexpr float epsilon = 1.1920928955078125e-07f; // std::exp2f(-CAST_STACK_DEPTH);
    if (std::abs(ray.direction.x) < epsilon)
        ray.direction.x = std::copysign(epsilon, ray.direction.x);
    if (std::abs(ray.direction.y) < epsilon)
        ray.direction.y = std::copysign(epsilon, ray.direction.y);
    if (std::abs(ray.direction.z) < epsilon)
        ray.direction.z = std::copysign(epsilon, ray.direction.z);

    // Precompute the coefficients of tx(x), ty(y) and tz(z).
    // The octree is assumed to reside at coordinates [1, 2].
    glm::vec3 tCoef = 1.0f / -glm::abs(ray.direction);
    glm::vec3 tBias = tCoef * ray.origin;

    // Select octant mask to mirro the coordinate system so taht ray direction is negative along each axis
    uint32_t octantMask = 7;
    if (ray.direction.x > 0.0f) {
        octantMask ^= (1 << 0);
        tBias.x = 3.0f * tCoef.x - tBias.x;
    }
    if (ray.direction.y > 0.0f) {
        octantMask ^= (1 << 1);
        tBias.y = 3.0f * tCoef.y - tBias.y;
    }
    if (ray.direction.z > 0.0f) {
        octantMask ^= (1 << 2);
        tBias.z = 3.0f * tCoef.z - tBias.z;
    }

    // Initialize the current voxel to the first child of the root
    const DAGDescriptor* parent = m_dagRootNode;
    int idx = 0;
    glm::vec3 pos = glm::vec3(1.0f);
    int scale = CAST_STACK_DEPTH - 1;
    float scaleExp2 = 0.5f; // exp2f(scale - sMax)

    // Initialize the active span of t-values
    float tMin = maxComponent(2.0f * tCoef - tBias);
    float tMax = minComponent(tCoef - tBias);
    tMin = std::max(tMin, 0.0f);

    if (tMin >= tMax)
        return {};

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

    // Traverse voxels along the ray as long as the current voxel stays within the octree
    struct StackItem {
        const DAGDescriptor* parent;
        float tMax;
    };
    std::array<StackItem, CAST_STACK_DEPTH + 1> stack;

    while (scale < CAST_STACK_DEPTH) {
        // === INTERSECT ===
        // Determine the maximum t-value of the cube by evaluating tx(), ty() and tz() at its corner
        glm::vec3 tCorner = pos * tCoef - tBias;
        float tcMax = minComponent(tCorner);

        // Process voxel if the corresponding bit in the valid mask is set
        int childIndex = 7 - idx ^ octantMask;
        if (parent->isValid(childIndex) && tMin <= tMax) {
            // === INTERSECT ===
            float tvMax = std::min(tMax, tcMax);
            float half = scaleExp2 * 0.5f;
            glm::vec3 tCenter = half * tCoef + tCorner;

            if (parent->isLeaf(childIndex)) {
                break; // Line 231
            }

            if (tMin <= tvMax) {
                // === PUSH ===
                stack[scale] = { parent, tMax };

                // Find child descriptor corresponding to the current voxel
                parent = getChild(parent, childIndex);

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
            unsigned differingBits = 0;
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
            parent = stack[scale].parent;
            tMax = stack[scale].tMax;

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

    // Indicate miss if we are outside the octree
    if (scale >= CAST_STACK_DEPTH) {
        return {};
    } else {
        /*// Undo mirroring of the coordinate system
		if ((octantMask & (1 << 0)) == 0)
		pos.x = 3.0f - scaleExp2 - pos.x;
		if ((octantMask & (1 << 1)) == 0)
		pos.y = 3.0f - scaleExp2 - pos.y;
		if ((octantMask & (1 << 2)) == 0)
		pos.z = 3.0f - scaleExp2 - pos.z;

		glm::vec3 hitPos = glm::min(glm::max(ray.origin + tMin * ray.direction, pos.x + epsilon), pos.x + scaleExp2 - epsilon);*/

        // Output result
        return tMin;
    }
}

}
