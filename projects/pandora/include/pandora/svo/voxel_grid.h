#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <tuple>
#include <memory>

namespace pandora {

class VoxelGrid {
public:
    VoxelGrid(unsigned resolution);

    unsigned resolution() const;

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;

	void fillCube();
    void fillSphere();

    bool get(uint64_t x, uint64_t y, uint64_t z) const;
	bool getMorton(uint_fast32_t mortonCode) const;
    void set(uint64_t x, uint64_t y, uint64_t z, bool value);


	uint32_t* data() { return m_values.get(); };

private:
	//static std::vector<bool> createValues(const glm::uvec3& extent);
	static std::unique_ptr<uint32_t[]> createValues(const glm::u64vec3& extent);
	std::pair<uint32_t*, uint32_t> index(uint64_t x, uint64_t y, uint64_t z) const;

private:
	unsigned m_resolution;
    glm::u64vec3 m_extent;
    //std::vector<bool> m_values;
	std::unique_ptr<uint32_t[]> m_values;
};

}
