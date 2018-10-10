#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <array>
#include <iostream>
#include <morton.h>

namespace pandora {

VoxelGrid::VoxelGrid(unsigned resolution)
    : m_resolution(resolution)
    , m_extent(resolution, resolution, resolution)
    , m_values(createValues(m_extent))

{
}

unsigned VoxelGrid::resolution() const
{
    return m_resolution;
}

std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> VoxelGrid::generateSurfaceMesh() const
{
    std::vector<glm::vec3> positions;
    std::vector<glm::ivec3> triangles;

    for (int z = 0; z < (int)m_extent.z; z++) {
        for (int y = 0; y < (int)m_extent.y; y++) {
            for (int x = 0; x < (int)m_extent.x; x++) {
                // Skip empty voxels
                if (!get(x, y, z))
                    continue;

                // Whether this (filled) voxel lies on the edge of the surface
                bool surfaceCube = false;

                // Filled voxels at the edge of the grid are always part of the surface
                if (x == 0 || x == (int)m_extent.x - 1)
                    surfaceCube = true;
                if (y == 0 || y == (int)m_extent.y - 1)
                    surfaceCube = true;
                if (z == 0 || z == (int)m_extent.z - 1)
                    surfaceCube = true;

                glm::ivec3 p(x, y, z);
                if (!surfaceCube) {
                    // For each neighbour
                    for (int dim = 0; dim < 3; dim++) {
                        for (int delta = -1; delta <= 1; delta += 2) {
                            glm::ivec3 n = p; // Neighbour position
                            n[dim] += delta;

                            if (!get(n.x, n.y, n.z)) {
                                surfaceCube = true;
                                break;
                            }
                        }

                        if (surfaceCube)
                            break;
                    }
                }

                if (surfaceCube) {
                    // https://github.com/ddiakopoulos/tinyply/blob/master/source/example.cpp
                    const glm::vec3 cubePositions[] = {
                        { -1, -1, -1 }, { -1, -1, +1 }, { -1, +1, +1 }, { -1, +1, -1 },
                        { +1, -1, +1 }, { +1, -1, -1 }, { +1, +1, -1 }, { +1, +1, +1 },
                        { -1, -1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { -1, -1, +1 },
                        { +1, +1, -1 }, { -1, +1, -1 }, { -1, +1, +1 }, { +1, +1, +1 },
                        { -1, -1, -1 }, { -1, +1, -1 }, { +1, +1, -1 }, { +1, -1, -1 },
                        { -1, +1, +1 }, { -1, -1, +1 }, { +1, -1, +1 }, { +1, +1, +1 }
                    };

                    std::array quads = { glm::ivec4 { 0, 1, 2, 3 }, glm::ivec4 { 4, 5, 6, 7 }, glm::ivec4 { 8, 9, 10, 11 }, glm::ivec4 { 12, 13, 14, 15 }, glm::ivec4 { 16, 17, 18, 19 }, glm::ivec4 { 20, 21, 22, 23 } };

                    glm::ivec3 offset((int)positions.size());
                    for (auto& q : quads) {
                        triangles.push_back(glm::ivec3 { q.x, q.y, q.z } + offset);
                        triangles.push_back(glm::ivec3 { q.x, q.z, q.w } + offset);
                    }

                    for (int i = 0; i < 24; ++i) {
                        positions.push_back(cubePositions[i] + glm::vec3(2 * p));
                    }
                }
            }
        }
    }

    return { std::move(positions), std::move(triangles) };
}

void VoxelGrid::fillSphere()
{
    glm::u64vec3 halfExtent = m_extent / (uint64_t)2;
    int minDim = minDimension(halfExtent);
    int radius2 = halfExtent[minDim] * halfExtent[minDim];

    for (uint64_t z = 0; z < m_extent.z; z++) {
        for (uint64_t y = 0; y < m_extent.y; y++) {
            for (uint64_t x = 0; x < m_extent.x; x++) {
                int xi = x - halfExtent.x;
                int yi = y - halfExtent.y;
                int zi = z - halfExtent.z;
                set(x, y, z, (xi * xi + yi * yi + zi * zi) < radius2);
            }
        }
    }
}

void VoxelGrid::fillCube()
{
    for (uint64_t z = 0; z < m_extent.z; z++) {
        for (uint64_t y = 3; y < m_extent.y; y++) {
            for (uint64_t x = 0; x < m_extent.x; x++) {
                set(x, y, z, true);
            }
        }
    }
}

bool VoxelGrid::get(uint64_t x, uint64_t y, uint64_t z) const
{
    const auto [pBlock, bit] = index(x, y, z);
    return *pBlock & (1 << bit);
}

bool VoxelGrid::getMorton(uint_fast32_t mortonCode) const
{
    uint_fast16_t x, y, z;
    libmorton::morton3D_32_decode(mortonCode, x, y, z);
    return get(x, y, z);
}

void VoxelGrid::set(uint64_t x, uint64_t y, uint64_t z, bool value)
{
    const auto [pBlock, bit] = index(x, y, z);
    if (value)
        *pBlock |= (1 << bit);
    else
        *pBlock &= ~(1 << bit);
}

std::pair<uint32_t*, uint32_t> VoxelGrid::index(uint64_t x, uint64_t y, uint64_t z) const
{
    ALWAYS_ASSERT(x >= 0 && x < m_extent.x);
    ALWAYS_ASSERT(y >= 0 && y < m_extent.y);
    ALWAYS_ASSERT(z >= 0 && z < m_extent.z);
    uint64_t bitPosition = z * m_extent.x * m_extent.y + y * m_extent.x + x;

    uint64_t block = bitPosition >> 5; // = bitPosition / 32
    uint32_t bitInBlock = static_cast<uint32_t>(bitPosition & ((1 << 5) - 1)); // = bitPosition % 32
    return { m_values.get() + block, bitInBlock };
}

/*std::vector<bool> VoxelGrid::createValues(const glm::uvec3& extent)
{
    int size = extent.x * extent.y * extent.z;
    return std::vector<bool>(size, false);
}*/

std::unique_ptr<uint32_t[]> VoxelGrid::createValues(const glm::u64vec3& extent)
{
    uint64_t sizeInBits = extent.x * extent.y * extent.z;
    uint64_t sizeInBlocks = (sizeInBits + 31) / 32;
    auto result = std::make_unique<uint32_t[]>(sizeInBlocks);
    std::fill(result.get(), result.get() + sizeInBlocks, 0);
    return result;
}

}
