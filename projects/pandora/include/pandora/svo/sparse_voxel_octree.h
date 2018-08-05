#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include <vector>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <glm/glm.hpp>
#include "sparse_voxel_octree_traversal_ispc.h"

namespace pandora
{

class SparseVoxelOctree
{
public:
	SparseVoxelOctree(const VoxelGrid& grid);
	~SparseVoxelOctree() = default;

	void intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const;
	std::optional<float> intersectScalar(Ray ray) const;

	std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> generateSurfaceMesh() const;
protected:
	struct SVOChildDescriptor
	{
		uint8_t leafMask;
		uint8_t validMask;
		uint16_t childPtr;

		inline bool isEmpty() const { return validMask == 0x0; };
		inline bool isFilled() const { return (validMask & leafMask) == 0xFF; };
		inline bool isValid(int i) const { return validMask & (1 << i); };
		inline bool isLeaf(int i) const { return (validMask & leafMask) & (1 << i); };
	};
	static_assert(sizeof(SVOChildDescriptor) == 4);

	std::vector<int> m_nodesPerDepthLevel;

	int m_resolution;
	SVOChildDescriptor m_svoRootNode;
	SVOChildDescriptor getChild(const SVOChildDescriptor& descriptor, int idx) const;
private:
	// SVO construction
	static SVOChildDescriptor createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask);
	static SVOChildDescriptor makeInnerNode(uint16_t baseIndex, gsl::span<SVOChildDescriptor, 8> children);
	std::pair<uint16_t, int> storeDescriptors(gsl::span<SparseVoxelOctree::SVOChildDescriptor> children);
private:
	std::vector<SVOChildDescriptor> m_allocator;
};

}