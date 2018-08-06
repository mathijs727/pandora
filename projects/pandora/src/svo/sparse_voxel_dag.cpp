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
    : m_resolution(grid.resolution())
{
    //m_rootNode = constructSVO(grid);
    m_rootNode = constructSVOBreadthFirst(grid);

    std::cout << "Size of SparseVoxelDAG: " << m_allocator.size() * sizeof(decltype(m_allocator)::value_type) << " bytes" << std::endl;
}

const SparseVoxelDAG::Descriptor* SparseVoxelDAG::constructSVOBreadthFirst(const VoxelGrid& grid)
{
    ALWAYS_ASSERT(isPowerOf2(m_resolution), "Resolution must be a power of 2"); // Resolution = power of 2
    int depth = intLog2(m_resolution) - 1;

    struct NodeInfoN1 {
        uint_fast32_t mortonCode; // Morton code (in level N-1)
            // Offset to the descriptor in m_allocator. If not set than this is node is fully filled which should
            //  be propegated up the tree.
        std::optional<NodeOffset> descriptorOffset;
    };
    std::vector<NodeInfoN1> previousLevelNodes;
    std::vector<NodeInfoN1> currentLevelNodes;

    // Creates and inserts leaf nodes
    uint_fast32_t finalMortonCode = static_cast<uint_fast32_t>(m_resolution * m_resolution * m_resolution);
    for (uint_fast32_t mortonCode = 0; mortonCode < finalMortonCode; mortonCode += 8) {
        // Create leaf node from 2x2x2 voxel block
        std::array<bool, 8> leafMask;
        std::array<bool, 8> validMask;
        for (uint_fast32_t i = 0; i < 8; i++) {
            bool v = grid.getMorton(mortonCode + i);
            validMask[i] = v;
            leafMask[i] = v;
        }
        auto desc = createStagingDescriptor(validMask, leafMask);
        if (desc.isFilledLeaf()) {
            currentLevelNodes.push_back({ mortonCode >> 3, {} });
        } else if (!desc.isEmpty()) {
            currentLevelNodes.push_back({ mortonCode >> 3, static_cast<NodeOffset>(m_allocator.size()) });
            m_allocator.push_back(static_cast<NodeOffset>(desc));
        }
    }

    auto createAndStoreDescriptor = [&](uint8_t validMask, uint8_t leafMask, gsl::span<NodeOffset> childrenOffsets) -> NodeOffset {
        Descriptor d;
        d.validMask = validMask;
        d.leafMask = leafMask;
        assert(d.numInnerNodeChildren() == childrenOffsets.size());

        auto offsetInAllocator = static_cast<NodeOffset>(m_allocator.size());
        m_allocator.push_back(static_cast<NodeOffset>(d));
        
		// Store child offsets directly after the descriptor itself
		m_allocator.insert(std::end(m_allocator), std::begin(childrenOffsets), std::end(childrenOffsets));

        return offsetInAllocator;
    };

    // Separate vector so m_allocator data doesnt change while inserting new descriptors.
    NodeOffset finalNodeOffset = 0;
    for (int N = 0; N < depth; N++) {
        std::swap(previousLevelNodes, currentLevelNodes);
        currentLevelNodes.clear();

        uint8_t validMask = 0x00;
        uint8_t leafMask = 0x00;
        eastl::fixed_vector<NodeOffset, 8> children;

        uint_fast32_t prevMortonCode = previousLevelNodes[0].mortonCode >> 3;
        for (unsigned i = 0; i < previousLevelNodes.size(); i++) {
            const auto& childNodeInfo = previousLevelNodes[i];

            auto mortonCodeN1 = childNodeInfo.mortonCode;
            auto mortonCodeN = mortonCodeN1 >> 3; // Morton code of this node (level N)
            if (prevMortonCode != mortonCodeN) {

                if (leafMask == 0xFF) {
                    currentLevelNodes.push_back({ prevMortonCode, {} });
                } else {
                    // Different morton code: we are finished with the previous node => store it
                    auto offset = createAndStoreDescriptor(validMask, leafMask, children);
                    currentLevelNodes.push_back({ prevMortonCode, offset });
                }

                validMask = 0;
				leafMask = 0;
                children.clear();
                prevMortonCode = mortonCodeN;
            }

            auto idx = mortonCodeN1 & ((1 << 3) - 1); // Right most 3 bits
            assert((validMask & (1 << idx)) == 0); // We should never visit the same child twice
            validMask |= 1 << idx;
            if (childNodeInfo.descriptorOffset) {
                children.push_back(*childNodeInfo.descriptorOffset);
			} else {
                leafMask |= 1 << idx;
			}
        }

        // Store final descriptor
        if (leafMask == 0xFF) {
            currentLevelNodes.push_back({ prevMortonCode, {} });
        } else {
            auto offset = createAndStoreDescriptor(validMask, leafMask, children);
            auto lastNodeMortonCode = (previousLevelNodes.back().mortonCode >> 3);
            assert(lastNodeMortonCode == prevMortonCode);
            currentLevelNodes.push_back({ prevMortonCode, offset });
            finalNodeOffset = offset; // Keep track of the offset to the root node
        }
    }

    assert(currentLevelNodes.size() == 1);
    return reinterpret_cast<const Descriptor*>(&m_allocator[finalNodeOffset]);
}

const SparseVoxelDAG::Descriptor* SparseVoxelDAG::constructSVO(const VoxelGrid& grid)
{
    uint_fast32_t resolution = static_cast<uint_fast32_t>(grid.resolution());
    int depth = intLog2(resolution) - 1;
    uint_fast32_t finalMortonCode = resolution * resolution * resolution;

    uint_fast32_t inputMortonCode = 0;
    auto consume = [&]() {
        // Create leaf node from 2x2x2 voxel block
        std::array<bool, 8> leafMask;
        std::array<bool, 8> validMask;
        for (int i = 0; i < 8; i++) {
            bool v = grid.getMorton(inputMortonCode++);
            validMask[i] = v;
            leafMask[i] = v;
        }
        return createStagingDescriptor(validMask, leafMask);
    };
    auto hasInput = [&]() {
        return inputMortonCode < finalMortonCode;
    };

    std::vector<eastl::fixed_vector<SVOConstructionQueueItem, 8>> queues(depth + 1);
    while (hasInput()) {
        Descriptor l = consume();
        queues[depth].push_back({ l, {} });
        int d = depth;

        while (d > 0 && queues[d].full()) {
            // Store in allocator (as opposed to store to disk) and create new inner node
            auto childIndices = storeDescriptors(queues[d]); // Store first so we know the base index (index of first child)
            Descriptor p = makeInnerNode(queues[d]);

            queues[d].clear();
            queues[d - 1].push_back({ p, childIndices });
            d--;
        }
    }

    ALWAYS_ASSERT(queues[0].size() == 1, "LOGIC ERROR");
    auto rootNodeIndex = storeDescriptors(queues[0])[0];
    return reinterpret_cast<const Descriptor*>(&m_allocator[rootNodeIndex]);
}

SparseVoxelDAG::Descriptor SparseVoxelDAG::createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask)
{
    // Create bit masks
    uint8_t leafMaskBits = 0x0;
    for (int i = 0; i < 8; i++)
        if (leafMask[i])
            leafMaskBits |= (1 << i);

    uint8_t validMaskBits = 0x0;
    for (int i = 0; i < 8; i++)
        if (validMask[i])
            validMaskBits |= (1 << i);

    // Create temporary descriptor
    Descriptor descriptor;
    descriptor.validMask = validMaskBits;
    descriptor.leafMask = leafMaskBits;
    return descriptor;
}

SparseVoxelDAG::Descriptor SparseVoxelDAG::makeInnerNode(gsl::span<SVOConstructionQueueItem, 8> children)
{
    std::array<bool, 8> validMask;
    std::array<bool, 8> leafMask;
    for (int i = 0; i < 8; i++) {
        const auto& child = children[i].descriptor;
        if (child.isEmpty()) {
            // No children => empty
            validMask[i] = false;
            leafMask[i] = false;
        } else if (child.isFilledLeaf()) {
            // All children are leafs => leaf
            validMask[i] = true;
            leafMask[i] = true;
        } else {
            // One or more children and not 8 leaf children
            validMask[i] = true;
            leafMask[i] = false;
        }
    }
    return createStagingDescriptor(validMask, leafMask);
}

eastl::fixed_vector<SparseVoxelDAG::NodeOffset, 8> SparseVoxelDAG::storeDescriptors(gsl::span<SVOConstructionQueueItem> items)
{
    eastl::fixed_vector<NodeOffset, 8> descriptorOffsets;
    for (const auto& item : items) { // For each descriptor
        const auto& descriptor = item.descriptor;
        const auto& childDescriptorOffsets = item.childDescriptorOffsets;

        if (!descriptor.isEmpty() && !descriptor.isFilledLeaf()) { // Not all empty or all filled => inner node
            descriptorOffsets.push_back(static_cast<NodeOffset>(m_allocator.size()));
            m_allocator.push_back(static_cast<NodeOffset>(descriptor));

			// Store child offsets directly after the descriptor itself
			m_allocator.insert(std::end(m_allocator), std::begin(childDescriptorOffsets), std::end(childDescriptorOffsets));
        }
    }
    assert(m_allocator.size() < (1 << 16));
    return descriptorOffsets;
}

const SparseVoxelDAG::Descriptor* SparseVoxelDAG::getChild(const Descriptor* descriptor, int idx) const
{
    uint32_t childMask = (descriptor->validMask ^ descriptor->leafMask) & ((1 << idx) - 1);
    uint32_t activeChildIndex = _mm_popcnt_u64(childMask);

    const NodeOffset* firstChildPtr = reinterpret_cast<const NodeOffset*>(descriptor) + 1;

	NodeOffset childOffset = *(firstChildPtr + activeChildIndex);
	return reinterpret_cast<const Descriptor*>(&m_allocator[childOffset]);
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
    const Descriptor* parent = m_rootNode;
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
    std::array<const Descriptor*, CAST_STACK_DEPTH + 1> stack;

    while (scale < CAST_STACK_DEPTH) {
        // === INTERSECT ===
        // Determine the maximum t-value of the cube by evaluating tx(), ty() and tz() at its corner
        glm::vec3 tCorner = pos * tCoef - tBias;
        float tcMax = minComponent(tCorner);

        // Process voxel if the corresponding bit in the valid mask is set
        int childIndex = 7 - idx ^ octantMask;
        if (parent->isValid(childIndex)) { // && tMin <= tMax) {
            // === INTERSECT ===
            float half = scaleExp2 * 0.5f;
            glm::vec3 tCenter = half * tCoef + tCorner;

            if (parent->isLeaf(childIndex)) {
                break; // Line 231
            }

            // === PUSH ===
            stack[scale] = parent;

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

            continue;
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
            parent = stack[scale];

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

std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> SparseVoxelDAG::generateSurfaceMesh() const
{
    std::vector<glm::vec3> positions;
    std::vector<glm::ivec3> triangles;

    struct StackItem {
        const Descriptor* descriptor;
        glm::ivec3 start;
        int extent;
    };
    std::vector<StackItem> stack = { { m_rootNode, glm::ivec3(0), m_resolution } };
    while (!stack.empty()) {
        auto stackItem = stack.back();
        stack.pop_back();

        // Loop visit in morton order?
        int halfExtent = stackItem.extent / 2;

        //int childID = 0;
        for (uint_fast32_t i = 0; i < 8; i++) {
            uint_fast16_t x, y, z;
            libmorton::morton3D_32_decode(i, x, y, z);
            glm::ivec3 cubeStart = stackItem.start + glm::ivec3(x * halfExtent, y * halfExtent, z * halfExtent);

            if (stackItem.descriptor->isValid(i)) {
                if (!stackItem.descriptor->isLeaf(i)) {
                    //uint32_t childOffset = *(reinterpret_cast<const uint32_t*>(stackItem.descriptor) + childID++);
                    //const auto* childDescriptor = reinterpret_cast<const Descriptor*>(&m_allocator[childOffset]);
                    const auto* childDescriptor = getChild(stackItem.descriptor, i);
                    stack.push_back(StackItem { childDescriptor, cubeStart, halfExtent });
                } else {
                    // https://github.com/ddiakopoulos/tinyply/blob/master/source/example.cpp
                    const glm::vec3 cubePositions[] = {
                        { 0, 0, 0 }, { 0, 0, +1 }, { 0, +1, +1 }, { 0, +1, 0 },
                        { +1, 0, +1 }, { +1, 0, 0 }, { +1, +1, 0 }, { +1, +1, +1 },
                        { 0, 0, 0 }, { +1, 0, 0 }, { +1, 0, +1 }, { 0, 0, +1 },
                        { +1, +1, 0 }, { 0, +1, 0 }, { 0, +1, +1 }, { +1, +1, +1 },
                        { 0, 0, 0 }, { 0, +1, 0 }, { +1, +1, 0 }, { +1, 0, 0 },
                        { 0, +1, +1 }, { 0, 0, +1 }, { +1, 0, +1 }, { +1, +1, +1 }
                    };
                    std::array quads = { glm::ivec4 { 0, 1, 2, 3 }, glm::ivec4 { 4, 5, 6, 7 }, glm::ivec4 { 8, 9, 10, 11 }, glm::ivec4 { 12, 13, 14, 15 }, glm::ivec4 { 16, 17, 18, 19 }, glm::ivec4 { 20, 21, 22, 23 } };

                    //if (halfExtent > 1)
                    //	continue;

                    glm::ivec3 offset((int)positions.size());
                    for (auto& q : quads) {
                        triangles.push_back(glm::ivec3 { q.x, q.y, q.z } + offset);
                        triangles.push_back(glm::ivec3 { q.x, q.z, q.w } + offset);
                    }

                    for (int i = 0; i < 24; ++i) {
                        positions.push_back(glm::vec3(cubeStart) + static_cast<float>(halfExtent) * cubePositions[i]);
                    }
                }
            }
        }
    }
    return { std::move(positions), std::move(triangles) };
}

}
