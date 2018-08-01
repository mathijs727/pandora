#pragma once
#include "pandora/core/pandora.h"
#include "pandora/utility/contiguous_allocator_ts.h"
#include <vector>
#include <gsl/span>

namespace pandora
{



class SparseVoxelOctree
{
public:
	SparseVoxelOctree(const VoxelGrid& grid);
	~SparseVoxelOctree() = default;
private:
	struct ChildDescriptor
	{
		//uint32_t childPtr : 16;
		//uint32_t validMask : 8;
		//uint32_t leafMask : 8;
		uint16_t childPtr;
		uint8_t validMask;
		uint8_t leafMask;

		inline bool isEmpty() const { return validMask == 0x0; };
	};
	static_assert(sizeof(ChildDescriptor) == 4);
	
	static ChildDescriptor createStagingDescriptor(gsl::span<bool, 8> validMask, gsl::span<bool, 8> leafMask);
	static ChildDescriptor makeInnerNode(uint16_t baseIndex, gsl::span<ChildDescriptor, 8> children);
	uint16_t storeDescriptors(gsl::span<SparseVoxelOctree::ChildDescriptor> children);
private:
	//ContiguousAllocatorTS<ChildDescriptor> m_allocator;
	std::vector<ChildDescriptor> m_allocator;
};

}