#pragma once
#include "pandora/core/pandora.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include <vector>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <glm/glm.hpp>

namespace pandora {

class SparseVoxelDAG : SparseVoxelOctree
{
public:
	SparseVoxelDAG(const VoxelGrid& grid);
	~SparseVoxelDAG() = default;

	//void intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const;
	std::optional<float> intersectScalar(Ray ray) const;

private:
	// NOTE: child pointers are stored directly after the descriptor
	struct DAGDescriptor
	{
		uint8_t leafMask;
		uint8_t validMask;
		uint16_t __padding;

		inline bool isEmpty() const { return validMask == 0x0; };
		inline bool isFilled() const { return (validMask & leafMask) == 0xFF; };
		inline bool isValid(int i) const { return validMask & (1 << i); };
		inline bool isLeaf(int i) const { return (validMask & leafMask) & (1 << i); };

		DAGDescriptor() = default;
		inline explicit DAGDescriptor(const SVOChildDescriptor& v) { leafMask = v.leafMask; validMask = v.validMask; __padding = 0; };
		inline explicit DAGDescriptor(uint32_t v) { memcpy(this, &v, sizeof(uint32_t)); };
		inline explicit operator uint32_t() { return *reinterpret_cast<const uint32_t*>(this); };
	};
	static_assert(sizeof(DAGDescriptor) == sizeof(uint32_t));

	uint16_t copySVO(SVOChildDescriptor descriptor);

	// CAREFULL: don't use this function during DAG construction (while m_allocator is touched)!
	const DAGDescriptor* getChild(const DAGDescriptor* descriptor, int idx) const;

	uint16_t storeDescriptor(DAGDescriptor descriptor);
	uint16_t storeDescriptor(DAGDescriptor descriptor, gsl::span<uint16_t> children);
private:
	const DAGDescriptor* m_dagRootNode;
	std::vector<uint32_t> m_allocator;
};

}