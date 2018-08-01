#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/math.h"
#include <EASTL/fixed_vector.h>
#include <array>
#include <cmath>
#include <libmorton/morton.h>

namespace pandora {

// http://graphics.cs.kuleuven.be/publications/BLD13OCCSVO/BLD13OCCSVO_paper.pdf
SparseVoxelOctree::SparseVoxelOctree(const VoxelGrid& grid)
    : m_resolution(grid.resolution())
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

    m_rootNode = queues[0][0];
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
        if (child.isEmpty()) {
            // No children => empty
            validMask[i] = false;
            leafMask[i] = false;
        } else if (child.isFilled()) {
            // All children are leafs => leaf
            validMask[i] = true;
            leafMask[i] = true;
        } else {
            // One or more children and not 8 leaf children
            validMask[i] = true;
            leafMask[i] = false;
        }
    }
    auto descriptor = createStagingDescriptor(validMask, leafMask);
    descriptor.childPtr = baseIndex;
    return descriptor;
}

uint16_t SparseVoxelOctree::storeDescriptors(gsl::span<SparseVoxelOctree::ChildDescriptor> children)
{
    uint16_t baseIndex = static_cast<uint16_t>(m_allocator.size());
    for (const auto& descriptor : children) {
        if (!descriptor.isEmpty() && !descriptor.isFilled()) // Not all empty or all filled => inner node
        {
            m_allocator.push_back(descriptor);
        }
    }
    return baseIndex;
}

std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> SparseVoxelOctree::generateSurfaceMesh() const
{
    std::vector<glm::vec3> positions;
    std::vector<glm::ivec3> triangles;

    struct StackItem {
        ChildDescriptor descriptor;
        glm::ivec3 start;
        int extent;
    };
    std::vector<StackItem> stack = { { m_rootNode, glm::ivec3(0), m_resolution } };
    while (!stack.empty()) {
        auto stackItem = stack.back();
        stack.pop_back();

        // Loop visit in morton order?
        int halfExtent = stackItem.extent / 2;

        int childID = 0;
        for (uint_fast32_t i = 0; i < 8; i++) {
            uint_fast16_t x, y, z;
            libmorton::morton3D_32_decode(i, x, y, z);
            glm::ivec3 cubeStart = stackItem.start + glm::ivec3(x * halfExtent, y * halfExtent, z * halfExtent);

            if (stackItem.descriptor.isValid(i)) {
                if (!stackItem.descriptor.isLeaf(i)) {
                    stack.push_back(StackItem { m_allocator[stackItem.descriptor.childPtr + childID++], cubeStart, halfExtent });
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
