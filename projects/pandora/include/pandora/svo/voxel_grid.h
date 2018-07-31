#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <tuple>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(int resolution);

	int resolution() const;

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

    void fillSphere();

    bool get(int x, int y, int z) const;
    void set(int x, int y, int z, bool value);


private:
    static std::vector<bool> createValues(const glm::uvec3& extent);
	int index(int x, int y, int z) const;

private:
	int m_resolution;
    glm::ivec3 m_extent;
    std::vector<bool> m_values;
};

}
