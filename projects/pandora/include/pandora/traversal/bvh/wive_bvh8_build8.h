#pragma once
#include "wive_bvh8.h"
#include <variant>

namespace pandora {

template <typename LeafObj>
class WiVeBVH8Build8 : public WiVeBVH8<LeafObj> {
public:
	void commit() override final;
private:
	// Pair: [handle, isLeaf]
	struct ConstructionBVHNode;
	struct InnerNodeHandle
	{
		uint32_t handle;
	};
	struct LeafNodeHandle
	{
		uint32_t handle;
		uint32_t primitiveCount;
	};
	std::variant<InnerNodeHandle, LeafNodeHandle> finalTreeFromConstructionTree(const ConstructionBVHNode* node);
	static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
	static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
	static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);
	static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);
private:
	struct ConstructionBVHNode {
		virtual void f() {}; // Need a virtual function to use dynamic_cast
	};

	struct ConstructionInnerNode : public ConstructionBVHNode {
		eastl::fixed_vector<Bounds, 8> childBounds;
		eastl::fixed_vector<ConstructionInnerNode*, 8> children;
	};

	struct ConstructionLeafNode : public ConstructionBVHNode {
		eastl::fixed_vector<std::pair<uint32_t, uint32_t>, 4> leafs;
	};
};

#include "wive_bvh8_build8_impl.h"

}
