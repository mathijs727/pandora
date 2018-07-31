#include "pandora/svo/voxel_grid.h"
#include "pandora/utility/math.h"
#include <array>

namespace pandora {

VoxelGrid::VoxelGrid(int resolution)
    : m_resolution(resolution)
    , m_extent(resolution, resolution, resolution)
    , m_values(createValues(m_extent))

{
}

int VoxelGrid::resolution() const
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

    return { positions, triangles };
}

void VoxelGrid::fillSphere()
{
    glm::ivec3 halfExtent = m_extent / 2;
    int minDim = minDimension(halfExtent);
    int radius2 = halfExtent[minDim] * halfExtent[minDim];

    for (int z = 0; z < (int)m_extent.z; z++) {
        for (int y = 0; y < (int)m_extent.y; y++) {
            for (int x = 0; x < (int)m_extent.x; x++) {
                int xi = x - halfExtent.x;
                int yi = y - halfExtent.y;
                int zi = z - halfExtent.z;
                set(x, y, z, (xi * xi + yi * yi + zi * zi) < radius2);
            }
        }
    }
}

bool VoxelGrid::get(int x, int y, int z) const
{
    return m_values[index(x, y, z)];
}

void VoxelGrid::set(int x, int y, int z, bool value)
{
    m_values[index(x, y, z)] = value;
}

int VoxelGrid::index(int x, int y, int z) const
{
    assert(x >= 0 && x < m_extent.x);
    assert(y >= 0 && y < m_extent.y);
    assert(z >= 0 && z < m_extent.z);
    return z * m_extent.x * m_extent.y + y * m_extent.x + x;
}

std::vector<bool> VoxelGrid::createValues(const glm::uvec3& extent)
{
    int size = extent.x * extent.y * extent.z;
    return std::vector<bool>(size, false);
}
}
