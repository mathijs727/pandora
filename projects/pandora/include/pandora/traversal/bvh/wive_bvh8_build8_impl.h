#include "pandora/utility/error_handling.h"

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::commit()
{
	RTCDevice device = rtcNewDevice(nullptr);
	rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
	RTCBVH bvh = rtcNewBVH(device);

	RTCBuildArguments arguments = rtcDefaultBuildArguments();
	arguments.byteSize = sizeof(arguments);
	arguments.buildFlags = RTC_BUILD_FLAG_NONE;
	arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
	arguments.maxBranchingFactor = 8;
	arguments.minLeafSize = 1;
	arguments.maxLeafSize = 4;
	arguments.bvh = bvh;
	arguments.primitives = m_primitives.data();
	arguments.primitiveCount = m_primitives.size();
	arguments.primitiveArrayCapacity = m_primitives.capacity();
	arguments.createNode = innerNodeCreate;
	arguments.setNodeChildren = innerNodeSetChildren;
	arguments.setNodeBounds = innerNodeSetBounds;
	arguments.createLeaf = leafCreate;
	arguments.userPtr = this;

	auto* constructionTreeRootNode = reinterpret_cast<ConstructionBVHNode*>(rtcBuildBVH(&arguments));

	m_innerNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHNode>>((uint32_t)m_primitives.size() * 2, 16);
	m_leafNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHLeaf>>((uint32_t)m_primitives.size() * 2, 16);

	auto nodeInfo = finalTreeFromConstructionTree(constructionTreeRootNode);
	if (!std::holds_alternative<InnerNodeHandle>(nodeInfo))
		THROW_ERROR("BVH root node may not be a leaf node!");

	m_rootHandle = std::get<InnerNodeHandle>(nodeInfo).handle;

	rtcReleaseBVH(bvh);
	rtcReleaseDevice(device);

	std::cout << "Actual prim count: " << m_primitives.size() << std::endl;
	m_primitives.clear();
	m_primitives.shrink_to_fit();

	testBVH();
}

template <typename LeafObj>
inline std::variant<typename WiVeBVH8Build8<LeafObj>::InnerNodeHandle, typename WiVeBVH8Build8<LeafObj>::LeafNodeHandle> WiVeBVH8Build8<LeafObj>::finalTreeFromConstructionTree(const ConstructionBVHNode* p)
{
	if (const auto* nodePtr = dynamic_cast<const ConstructionInnerNode*>(p)) {
		// Allocate node
		auto[finalNodeHandle, finalNodePtr] = m_innerNodeAllocator->allocate();
		size_t numChildren = nodePtr->children.size();

		// Copy child data
		std::array<float, 8> minX, minY, minZ, maxX, maxY, maxZ;
		std::array<uint32_t, 8> children, permutationOffsets; // Permutation offsets and child flags
		for (size_t i = 0; i < numChildren; i++) {
			minX[i] = nodePtr->childBounds[i].min.x;
			minY[i] = nodePtr->childBounds[i].min.y;
			minZ[i] = nodePtr->childBounds[i].min.z;
			maxX[i] = nodePtr->childBounds[i].max.x;
			maxY[i] = nodePtr->childBounds[i].max.y;
			maxZ[i] = nodePtr->childBounds[i].max.z;
			auto childInfo = finalTreeFromConstructionTree(nodePtr->children[i]);
			if (std::holds_alternative<InnerNodeHandle>(childInfo)) {
				children[i] = compressHandleInner(std::get<InnerNodeHandle>(childInfo).handle);
			} else {
				const auto& leafInfo = std::get<LeafNodeHandle>(childInfo);
				children[i] = compressHandleLeaf(leafInfo.handle, leafInfo.primitiveCount);
			}
		}
		for (size_t i = numChildren; i < 8; i++) {
			minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
			children[i] = compressHandleEmpty();
		}

		// Create permutations
		std::fill(std::begin(permutationOffsets), std::end(permutationOffsets), 0);
		for (int x = -1; x <= 1; x += 2) {
			for (int y = -1; y <= 1; y += 2) {
				for (int z = -1; z <= 1; z += 2) {
					glm::vec3 direction(x, y, z);

					std::array<uint32_t, 8> indices = { 0, 1, 2, 3, 4, 5, 6, 7 };
					std::array<float, 8> distances;
					std::transform(std::begin(indices), std::end(indices), std::begin(distances), [&](uint32_t i) -> float {
						if (i >= numChildren)
							return std::numeric_limits<float>::max();

						// Calculate the bounding box corner opposite to the direction vector
						const Bounds& bounds = nodePtr->childBounds[i];
						glm::vec3 extremePoint(x == -1 ? bounds.min[0] : bounds.max[0], y == -1 ? bounds.min[1] : bounds.max[1], z == -1 ? bounds.min[2] : bounds.max[2]);
						return glm::dot(extremePoint, direction);
					});
					// Sort bounding boxes based on their closest intersection to the (moving) plane in the given direction
					std::sort(std::begin(indices), std::end(indices), [&](uint32_t a, uint32_t b) -> bool {
						return distances[a] < distances[b];
					});

					uint32_t shiftAmount = signShiftAmount(x > 0, y > 0, z > 0);
					for (int i = 0; i < 8; i++)
						permutationOffsets[i] |= indices[i] << shiftAmount;
				}
			}
		}

		// Store data in SIMD format
		finalNodePtr->minX.load(minX);
		finalNodePtr->minY.load(minY);
		finalNodePtr->minZ.load(minZ);
		finalNodePtr->maxX.load(maxX);
		finalNodePtr->maxY.load(maxY);
		finalNodePtr->maxZ.load(maxZ);
		finalNodePtr->children.load(children);
		finalNodePtr->permutationOffsets.load(permutationOffsets);
		return InnerNodeHandle { finalNodeHandle };
	} else if (const auto* nodePtr = dynamic_cast<const ConstructionLeafNode*>(p)) {
		auto[finalNodeHandle, finalNodePtr] = m_leafNodeAllocator->allocate();
		uint32_t primitiveCount = (uint32_t)nodePtr->leafs.size();
		for (uint32_t i = 0; i < primitiveCount; i++) {
			finalNodePtr->leafObjectIDs[i] = std::get<0>(nodePtr->leafs[i]);
			finalNodePtr->primitiveIDs[i] = std::get<1>(nodePtr->leafs[i]);
		}
		for (uint32_t i = primitiveCount; i < 4; i++) {
			finalNodePtr->leafObjectIDs[i] = emptyHandle;
			finalNodePtr->primitiveIDs[i] = emptyHandle;
		}
		return LeafNodeHandle{ finalNodeHandle, primitiveCount };
	} else {
		THROW_ERROR("Unknown WiVe BVH construction node type!");
		return InnerNodeHandle{ 0 };
	}
}

template <typename LeafObj>
inline void* WiVeBVH8Build8<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
	assert(numChildren <= 8);

	auto* self = reinterpret_cast<WiVeBVH8<LeafObj>*>(userPtr);
	void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode), std::alignment_of_v<ConstructionInnerNode>);
	return reinterpret_cast<void*>(new (ptr) ConstructionInnerNode);
}

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
	assert(numChildren <= 8);

	auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
	for (unsigned childID = 0; childID < numChildren; childID++) {
		auto* child = reinterpret_cast<ConstructionInnerNode*>(childPtr[childID]);
		node->children.push_back(child);
	}
}

template <typename LeafObj>
inline void WiVeBVH8Build8<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
	assert(numChildren <= 8);

	auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
	for (unsigned childID = 0; childID < numChildren; childID++) {
		const RTCBounds* embreeBounds = bounds[childID];
		node->childBounds.emplace_back(
			glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
			glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
	}
}

template <typename LeafObj>
inline void* WiVeBVH8Build8<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
	assert(numPrims <= 4);

	auto* self = reinterpret_cast<WiVeBVH8<LeafObj>*>(userPtr);
	void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionLeafNode), std::alignment_of_v<ConstructionLeafNode>);

	ConstructionLeafNode* leafNode = new (ptr) ConstructionLeafNode();
	for (size_t i = 0; i < numPrims; i++) {
		leafNode->leafs.push_back({ prims[i].geomID, prims[i].primID });
	}
	return ptr;
}
