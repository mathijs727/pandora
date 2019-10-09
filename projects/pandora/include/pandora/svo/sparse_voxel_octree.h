#pragma once
#include "pandora/graphics_core/pandora.h"
#include <vector>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <glm/glm.hpp>

#ifdef PANDORA_ISPC_SUPPORT
#include "sparse_voxel_octree_traversal_ispc.h"
#endif

namespace pandora
{

class SparseVoxelOctree
{
public:
	SparseVoxelOctree(const VoxelGrid& grid);
	~SparseVoxelOctree() = default;

#ifdef PANDORA_ISPC_SUPPORT
	void intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const;
#endif
	std::optional<float> intersectScalar(Ray ray) const;

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;
private:
	struct ChildDescriptor
	{
		uint8_t leafMask;
		uint8_t validMask;
		uint16_t childPtr;

		inline bool isEmpty() const { return validMask == 0x0; };
		inline bool isFilled() const { return (validMask & leafMask) == 0xFF; };
		inline bool isValid(int i) const { return validMask & (1 << i); };
		inline bool isLeaf(int i) const { return (validMask & leafMask) & (1 << i); };
	};
	static_assert(sizeof(ChildDescriptor) == 4);

	ChildDescriptor getChild(const ChildDescriptor& descriptor, int idx) const;

	// SVO construction
	static ChildDescriptor createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask);
	static ChildDescriptor makeInnerNode(uint16_t baseIndex, gsl::span<ChildDescriptor, 8> children);
	uint16_t storeDescriptors(gsl::span<ChildDescriptor> children);
private:
	int m_resolution;
	ChildDescriptor m_rootNode;
	std::vector<ChildDescriptor> m_allocator;
};

}