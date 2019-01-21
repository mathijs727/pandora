#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <tuple>
#include <vector>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(int resolution);

    int resolution() const;

    std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

    void fillCube();
    void fillSphere();

    void fill()
    {
        for (int x = 0; x < m_resolution; x++) {
            for (int y = 0; y < m_resolution; y++) {
                for (int z = 0; z < m_resolution; z++) {
                    set(x, y, z, true);
                }
            }
        }
    }

    bool get(int x, int y, int z) const;
    bool getMorton(uint_fast32_t mortonCode) const;
    void set(int x, int y, int z, bool value);

    uint32_t* data() { return m_values.get(); };

private:
    //static std::vector<bool> createValues(const glm::uvec3& extent);
    static std::unique_ptr<uint32_t[]> createValues(const glm::ivec3& extent);
    std::pair<uint32_t*, uint32_t> index(int x, int y, int z) const;

private:
    int m_resolution;
    glm::ivec3 m_extent;
    //std::vector<bool> m_values;
    std::unique_ptr<uint32_t[]> m_values;
};

}
