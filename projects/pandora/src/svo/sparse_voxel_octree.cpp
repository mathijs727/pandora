#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/math.h"
#include <EASTL/fixed_vector.h>
#include <array>
#include <cmath>

namespace pandora {

// http://graphics.cs.kuleuven.be/publications/BLD13OCCSVO/BLD13OCCSVO_paper.pdf
SparseVoxelOctree::SparseVoxelOctree(const VoxelGrid& grid)
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
            validMask[i] = grid.getMorton(inputMortonCode++);
            leafMask[i] = true;
        }
        return createStagingDescriptor(validMask, leafMask);
    };
    auto hasInput = [&]() {
        return inputMortonCode < finalMortonCode;
    };

    std::vector<eastl::fixed_vector<ChildDescriptor, 8>> queues(depth + 1);
    while (hasInput()) {
        ChildDescriptor l = consume();
        queues[depth].push_back(l);
        int d = depth;

        while (d > 0 && queues[d].full()) {
			// Store in allocator (as opposed to store to disk) and create new inner node
			auto baseIndex = storeDescriptors(queues[d]); // Store first so we know the base index (index of first child)
            ChildDescriptor p = makeInnerNode(baseIndex, queues[d]);

			queues[d].clear();
			queues[d - 1].push_back(p);
			d--;
        }
    }
}

SparseVoxelOctree::ChildDescriptor SparseVoxelOctree::createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask)
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
    ChildDescriptor descriptor;
    descriptor.childPtr = 0;
    descriptor.validMask = validMaskBits;
    descriptor.leafMask = leafMaskBits;
    return descriptor;
}

SparseVoxelOctree::ChildDescriptor SparseVoxelOctree::makeInnerNode(uint16_t baseIndex, gsl::span<ChildDescriptor, 8> children)
{
	std::array<bool, 8> validMask;
	std::array<bool, 8> leafMask;
	for (int i = 0; i < 8; i++) {
		const ChildDescriptor& child = children[i];
		validMask[i] = (child.validMask != 0x0); // 0x0 => no children => empty child
		leafMask[i] = false;
	}
	auto descriptor = createStagingDescriptor(validMask, leafMask);
	descriptor.childPtr = baseIndex;
	return descriptor;
}

uint16_t SparseVoxelOctree::storeDescriptors(gsl::span<SparseVoxelOctree::ChildDescriptor> children)
{
    uint16_t baseIndex = static_cast<uint16_t>(m_allocator.size());
	for (const auto& descriptor : children) {
		if (descriptor.validMask != 0x0)
			m_allocator.push_back(descriptor);
	}
	return baseIndex;
}

}
