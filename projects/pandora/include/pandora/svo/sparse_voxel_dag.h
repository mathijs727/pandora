#pragma once
#include "pandora/core/pandora.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include <vector>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <glm/glm.hpp>
#include <EASTL/fixed_vector.h>

namespace pandora {

class SparseVoxelDAG
{
public:
	SparseVoxelDAG(const VoxelGrid& grid);
	~SparseVoxelDAG() = default;

	//void intersectSIMD(ispc::RaySOA rays, ispc::HitSOA hits, int N) const;
	std::optional<float> intersectScalar(Ray ray) const;

private:
	// NOTE: child pointers are stored directly after the descriptor
	struct Descriptor
	{
		uint8_t leafMask;
		uint8_t validMask;
		uint16_t __padding;

		inline bool isEmpty() const { return validMask == 0x0; };
		inline bool isFilled() const { return (validMask & leafMask) == 0xFF; };
		inline bool isValid(int i) const { return validMask & (1 << i); };
		inline bool isLeaf(int i) const { return (validMask & leafMask) & (1 << i); };

		Descriptor() = default;
		//inline explicit Descriptor(const SVOChildDescriptor& v) { leafMask = v.leafMask; validMask = v.validMask; __padding = 0; };
		inline explicit Descriptor(uint32_t v) { memcpy(this, &v, sizeof(uint32_t)); };
		inline explicit operator uint32_t() const { return *reinterpret_cast<const uint32_t*>(this); };
	};
	static_assert(sizeof(Descriptor) == sizeof(uint32_t));

	const Descriptor* constructSVO(const VoxelGrid& grid);

	// CAREFULL: don't use this function during DAG construction (while m_allocator is touched)!
	const Descriptor* getChild(const Descriptor* descriptor, int idx) const;

	// SVO construction
	struct SVOConstructionQueueItem
	{
		Descriptor descriptor;
		eastl::fixed_vector<uint32_t, 8> childDescriptorIndices;
	};
	static Descriptor createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask);
	static Descriptor makeInnerNode(gsl::span<SVOConstructionQueueItem, 8> children);
	eastl::fixed_vector<uint32_t, 8> storeDescriptors(gsl::span<SVOConstructionQueueItem> children);

private:
	const Descriptor* m_rootNode;
	std::vector<uint32_t> m_allocator;
};

}