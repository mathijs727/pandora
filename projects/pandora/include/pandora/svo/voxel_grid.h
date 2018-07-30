#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <tuple>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(unsigned extent);
    VoxelGrid(unsigned width, unsigned height, unsigned depth);

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

    void fillSphere();

    bool get(unsigned x, unsigned y, unsigned z) const;
    void set(unsigned x, unsigned y, unsigned z, bool value);

    static std::vector<bool> createValues(const glm::uvec3& extent);

private:
    unsigned index(unsigned x, unsigned y, unsigned z) const;

private:
    glm::uvec3 m_extent;
    std::vector<bool> m_values;
};

}
