#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/core/ray.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <EASTL/fixed_vector.h>
#include <bitset>
#include <boost/functional/hash.hpp>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <immintrin.h>
#include <libmorton/morton.h>

namespace pandora {

// http://graphics.cs.kuleuven.be/publications/BLD13OCCSVO/BLD13OCCSVO_paper.pdf
SparseVoxelDAG::SparseVoxelDAG(const VoxelGrid& grid)
    : m_resolution(grid.resolution())
{
    //m_rootNode = constructSVO(grid);
    m_rootNodeOffset = constructSVOBreadthFirst(grid);
    m_allocator.shrink_to_fit();
    m_data = m_allocator.data();

    std::cout << "Size of SparseVoxelDAG before compression: " << m_allocator.size() * sizeof(decltype(m_allocator)::value_type) << " bytes" << std::endl;
}

SparseVoxelDAG::AbsoluteNodeOffset SparseVoxelDAG::constructSVOBreadthFirst(const VoxelGrid& grid)
{
    ALWAYS_ASSERT(isPowerOf2(m_resolution), "Resolution must be a power of 2"); // Resolution = power of 2
    int depth = intLog2(m_resolution) - 2;

    struct NodeInfoN1 {
        uint_fast32_t mortonCode; // Morton code (in level N-1)
        // Offset to the descriptor in m_allocator. If not set than this is node is fully filled which should
        //  be propegated up the tree.
		bool isLeaf;
		AbsoluteNodeOffset absoluteDescriptorOffset;
        //std::optional<std::pair<bool, AbsoluteNodeOffset>> absoluteDescriptorOffset;
    };
    std::vector<NodeInfoN1> previousLevelNodes;
    std::vector<NodeInfoN1> currentLevelNodes;

    // Creates and inserts leaf nodes
    uint_fast32_t finalMortonCode = static_cast<uint_fast32_t>(m_resolution * m_resolution * m_resolution);
	for (uint_fast32_t mortonCode = 0; mortonCode < finalMortonCode; mortonCode += 64) {
		// Create leaf node from 4x4x4 voxel block
		std::bitset<64> leaf;
		for (uint32_t i = 0; i < 64; i++) {
			leaf[i] = grid.getMorton(mortonCode + i);
		}

		if (leaf.any()) {
			currentLevelNodes.push_back({ mortonCode >> 6, true, m_leafAllocator.size() });
			m_leafAllocator.push_back(leaf.to_ullong());
		}
	}
    m_treeLevels.push_back({ 0, m_allocator.size() });

    auto createAndStoreDescriptor = [&](uint8_t validMask, uint8_t leafMask, const gsl::span<std::pair<bool, AbsoluteNodeOffset>> absoluteChildOffsets) -> size_t {
        Descriptor d;
        d.validMask = validMask;
        d.leafMask = leafMask;
        assert(d.numChildren() == absoluteChildOffsets.size());

        AbsoluteNodeOffset absoluteOffset = m_allocator.size();
        m_allocator.push_back(static_cast<RelativeNodeOffset>(d));

        // Store child offsets directly after the descriptor itself
        //m_allocator.insert(std::end(m_allocator), std::begin(childrenOffsets), std::end(childrenOffsets));
        for (auto [isLeaf, absoluteChildOffset] : absoluteChildOffsets) { // Offsets from the start of m_allocator
			if (isLeaf) {
				assert(absoluteChildOffset < m_leafAllocator.size());
				m_allocator.push_back(static_cast<RelativeNodeOffset>(absoluteChildOffset));
			} else {
				// Store as offset from current node (always negative)
				assert(absoluteOffset > absoluteChildOffset);
				m_allocator.push_back(static_cast<RelativeNodeOffset>(absoluteOffset - absoluteChildOffset));
			}
        }

        return absoluteOffset;
    };

    // Separate vector so m_allocator data doesnt change while inserting new descriptors.
    AbsoluteNodeOffset rootNodeOffset = 0;
    for (int N = 0; N < depth; N++) {
        std::swap(previousLevelNodes, currentLevelNodes);
        currentLevelNodes.clear();

        size_t currentLevelStart = m_allocator.size();

        uint8_t validMask = 0x00;
        uint8_t leafMask = 0x00;
        eastl::fixed_vector<std::pair<bool, AbsoluteNodeOffset>, 8> absoluteChildrenOffsets;

        uint_fast32_t prevMortonCode = previousLevelNodes[0].mortonCode >> 3;
        for (unsigned i = 0; i < previousLevelNodes.size(); i++) {
            const auto& childNodeInfo = previousLevelNodes[i];

            auto mortonCodeN1 = childNodeInfo.mortonCode;
            auto mortonCodeN = mortonCodeN1 >> 3; // Morton code of this node (level N)
            if (prevMortonCode != mortonCodeN) {
                // Different morton code: we are finished with the previous node => store it
                auto descriptorOffset = createAndStoreDescriptor(validMask, leafMask, absoluteChildrenOffsets);
				currentLevelNodes.push_back({ prevMortonCode, false, descriptorOffset });

                validMask = 0;
                leafMask = 0;
                absoluteChildrenOffsets.clear();
                prevMortonCode = mortonCodeN;
            }

            auto idx = mortonCodeN1 & ((1 << 3) - 1); // Right most 3 bits
            assert((validMask & (1 << idx)) == 0); // We should never visit the same child twice
            validMask |= 1 << idx;
			if (childNodeInfo.isLeaf)
				leafMask |= 1 << idx;
			absoluteChildrenOffsets.push_back({ childNodeInfo.isLeaf, childNodeInfo.absoluteDescriptorOffset });
        }

        // Store final descriptor
		{
            auto descriptorOffset = createAndStoreDescriptor(validMask, leafMask, absoluteChildrenOffsets);
            auto lastNodeMortonCode = (previousLevelNodes.back().mortonCode >> 3);
            assert(lastNodeMortonCode == prevMortonCode);
			currentLevelNodes.push_back({ prevMortonCode, false, descriptorOffset });
            rootNodeOffset = descriptorOffset; // Keep track of the offset to the root node
        }

        m_treeLevels.push_back({ currentLevelStart, m_allocator.size() });
    }

    assert(currentLevelNodes.size() == 1);
    return rootNodeOffset;
}

void compressDAGs(gsl::span<SparseVoxelDAG> svos)
{
    /*using Descriptor = SparseVoxelDAG::Descriptor;
    using RelativeNodeOffset = SparseVoxelDAG::RelativeNodeOffset;
    using AbsoluteNodeOffset = size_t;

    size_t maxTreeDepth = 0;
    maxTreeDepth = svos[0].m_treeLevels.size();
    for (const auto& svo : svos)
        assert(svo.m_treeLevels.size() == maxTreeDepth);

    //using FullDescriptor = std::pair<Descriptor, eastl::fixed_vector<AbsoluteNodeOffset, 8>>;
    struct FullDescriptor {
        Descriptor descriptor;
        eastl::fixed_vector<AbsoluteNodeOffset, 8> children;

        bool operator==(const FullDescriptor& other) const
        {
            return descriptor == other.descriptor && children.size() == other.children.size() && memcmp(children.data(), other.children.data(), children.size() * sizeof(decltype(children)::value_type)) == 0;
        }
    };

    struct FullDescriptorHasher {
        std::size_t operator()(const FullDescriptor& desc) const noexcept
        {
            // https://www.boost.org/doc/libs/1_67_0/doc/html/hash/combine.html
            size_t seed = 0;
            boost::hash_combine(seed, boost::hash<uint8_t> {}(desc.descriptor.leafMask));
            boost::hash_combine(seed, boost::hash<uint8_t> {}(desc.descriptor.validMask));
            for (size_t childOffset : desc.children)
                boost::hash_combine(seed, boost::hash<size_t> {}(childOffset));
            return seed;
        }
    };

    auto decodeDescriptor = [](const Descriptor* descriptorPtr, AbsoluteNodeOffset absoluteDescriptorOffset, const std::unordered_map<AbsoluteNodeOffset, size_t>& childDescriptorOffsetLUT) -> FullDescriptor {
        const auto* dataPtr = reinterpret_cast<const RelativeNodeOffset*>(descriptorPtr);
        const auto* childPtr = dataPtr + 1;

        FullDescriptor result;
        result.descriptor = *descriptorPtr;
        for (int i = 0; i < 8; i++) {
            if (descriptorPtr->isInnerNode(i)) {
                RelativeNodeOffset relativeChildOffset = *(childPtr++);
                assert(absoluteDescriptorOffset >= relativeChildOffset);
                AbsoluteNodeOffset absoluteChildOffset = absoluteDescriptorOffset - relativeChildOffset;
                size_t offsetInChildrenArray = childDescriptorOffsetLUT.find(absoluteChildOffset)->second;
                result.children.push_back(offsetInChildrenArray);
            }
        }
        return result;
    };

    auto nextDescriptor = [](const RelativeNodeOffset* descriptorPtr) -> size_t {
        Descriptor descriptor = *reinterpret_cast<const Descriptor*>(descriptorPtr);
        size_t offset = 1 + descriptor.numInnerNodeChildren();
        return offset;
    };

    std::vector<RelativeNodeOffset> allocator;
    auto storeDescriptor = [&](const FullDescriptor& fullDescriptor) -> AbsoluteNodeOffset {
        AbsoluteNodeOffset absoluteDescriptorOffset = static_cast<AbsoluteNodeOffset>(allocator.size());

        allocator.push_back(static_cast<RelativeNodeOffset>(fullDescriptor.descriptor));
        int numChildren = fullDescriptor.descriptor.numInnerNodeChildren();
        for (int i = 0; i < numChildren; i++) {
            AbsoluteNodeOffset absoluteChildOffset = fullDescriptor.children[i];
            RelativeNodeOffset relativeChildOffset = static_cast<RelativeNodeOffset>(absoluteDescriptorOffset - absoluteChildOffset);
            allocator.push_back(relativeChildOffset);
        }

        return absoluteDescriptorOffset;
    };

    std::unordered_map<FullDescriptor, AbsoluteNodeOffset, FullDescriptorHasher> lut; // Look-up table from descriptors (pointing into DAG allocator array) to nodes in the DAG allocator array

    for (auto& svo : svos) {
        std::unordered_map<AbsoluteNodeOffset, size_t> childDescriptorOffsetLUT; // Maps absolute offsets into encoded (NodeOffset*) array to offsets into the childDescriptors array
        std::unordered_map<AbsoluteNodeOffset, size_t> descriptorOffsetLUT; // Maps absolute offsets into encoded (NodeOffset*) array to offsets into the descriptors array
        std::vector<FullDescriptor> childDescriptors; // Descriptors pointing into DAG allocator array
        std::vector<FullDescriptor> descriptors; // Descriptors at the currrent level pointing into the children array

        for (size_t d = 0; d < maxTreeDepth; d++) {
            auto [start, end] = svo.m_treeLevels[d];

            std::swap(descriptorOffsetLUT, childDescriptorOffsetLUT);
            std::swap(descriptors, childDescriptors);
            descriptors.clear();
            descriptorOffsetLUT.clear();

            // Fill descriptors array & descriptorOffsetLUT
            {
                size_t descriptorOffset = start;

                while (descriptorOffset < end) {
                    const auto* dataPtr = svo.m_data + descriptorOffset;
                    const auto* descriptorPtr = reinterpret_cast<const Descriptor*>(dataPtr);

                    descriptorOffsetLUT[descriptorOffset] = descriptors.size(); // M
                    descriptors.push_back(decodeDescriptor(descriptorPtr, descriptorOffset, childDescriptorOffsetLUT));

                    descriptorOffset += nextDescriptor(dataPtr);
                }
            }

            // Update child pointers to pointers into the DAG allocator array; inserting any children that are not in the DAG allocator array yet.
            for (auto& mutDescriptor : descriptors) {
                for (auto& childOffset : mutDescriptor.children) {
                    const auto& child = childDescriptors[childOffset];
                    AbsoluteNodeOffset newNodeAbsoluteOffset = allocator.size();
                    if (auto [iter, succeeded] = lut.try_emplace(child, newNodeAbsoluteOffset); succeeded) {
                        AbsoluteNodeOffset offset = storeDescriptor(child); // Unique node => insert into DAG allocator array
                        assert(offset == iter->second);
                        childOffset = iter->second;
                    } else {
                        assert(iter != lut.end());
                        childOffset = iter->second;
                    }
                }
            }
        }

        assert(descriptors.size() == 1);
        svo.m_rootNodeOffset = storeDescriptor(descriptors[0]);
        svo.m_allocator.clear();
    }

    for (auto& svo : svos) {
        svo.m_data = allocator.data(); // Set this after all work on the allocator is done (and the pointer cant change because of reallocations)
    }
    svos[0].m_allocator = std::move(allocator);*/
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

#ifdef PANDORA_ISPC_SUPPORT
void SparseVoxelDAG::intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const
{
    static_assert(sizeof(Descriptor) == sizeof(uint16_t));
    static_assert(std::is_same_v<RelativeNodeOffset, uint16_t> || std::is_same_v<RelativeNodeOffset, uint32_t>);

    if constexpr (std::is_same_v<RelativeNodeOffset, uint16_t>) {
        ispc::SparseVoxelDAG16 svdag;
        svdag.descriptors = reinterpret_cast<const uint16_t*>(m_data); // Using contexpr if-statements dont fix this???
        svdag.rootNodeOffset = static_cast<uint32_t>(m_rootNodeOffset);
        ispc::SparseVoxelDAG16_intersect(svdag, rays, hits, N);
    }
    if constexpr (std::is_same_v<RelativeNodeOffset, uint32_t>) {
        ispc::SparseVoxelDAG32 svdag;
        svdag.descriptors = reinterpret_cast<const uint32_t*>(m_data); // Using contexpr if-statements dont fix this???
        svdag.rootNodeOffset = static_cast<uint32_t>(m_rootNodeOffset);
        ispc::SparseVoxelDAG32_intersect(svdag, rays, hits, N);
    }
}
#endif

const SparseVoxelDAG::Descriptor* SparseVoxelDAG::getChild(const Descriptor* descriptorPtr, int idx) const
{
	uint32_t childMask = descriptorPtr->validMask & ((1 << idx) - 1);
	uint32_t activeChildIndex = _mm_popcnt_u64(childMask);

	const RelativeNodeOffset* dataPtr = reinterpret_cast<const RelativeNodeOffset*>(descriptorPtr);
	const RelativeNodeOffset* firstChildPtr = dataPtr + 1;

	RelativeNodeOffset relativeChildOffset = *(firstChildPtr + activeChildIndex);
	return reinterpret_cast<const Descriptor*>(dataPtr - relativeChildOffset);
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
    const Descriptor* parent = reinterpret_cast<const Descriptor*>(m_data + m_rootNodeOffset);
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
        int childIndex = 7 - (idx ^ octantMask);
        if (parent->isValid(childIndex)) {
            if (parent->isLeaf(childIndex)) {
                break; // Line 231
            }

            float half = scaleExp2 * 0.5f;
            glm::vec3 tCenter = half * tCoef + tCorner;

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
    std::vector<StackItem> stack = { { reinterpret_cast<const Descriptor*>(m_data + m_rootNodeOffset), glm::ivec3(0), m_resolution } };
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
