#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <tuple>
#include <memory>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(int resolution);

	int resolution() const;

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

	void fillCube();
    void fillSphere();

    bool get(int x, int y, int z) const;
	bool getMorton(uint_fast32_t mortonCode) const;
    void set(int x, int y, int z, bool value);


	uint32_t* data() { return m_values.get(); };

private:
	//static std::vector<bool> createValues(const glm::uvec3& extent);
	static std::unique_ptr<uint32_t[]> createValues(const glm::uvec3& extent);
	std::pair<uint32_t*, int> index(int x, int y, int z) const;

private:
	int m_resolution;
    glm::ivec3 m_extent;
    //std::vector<bool> m_values;
	std::unique_ptr<uint32_t[]> m_values;
};

}
