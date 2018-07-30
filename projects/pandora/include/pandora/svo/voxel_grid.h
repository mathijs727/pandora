#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <tuple>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(int extent);
    VoxelGrid(int width, int height, int depth);

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

    void fillSphere();

    bool get(int x, int y, int z) const;
    void set(int x, int y, int z, bool value);

    static std::vector<bool> createValues(const glm::uvec3& extent);

private:
	int index(int x, int y, int z) const;

private:
    glm::ivec3 m_extent;
    std::vector<bool> m_values;
};

}
