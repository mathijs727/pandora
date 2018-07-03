

template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::commit()
{
	RTCDevice device = rtcNewDevice(nullptr);
	rtcSetDeviceErrorFunction(device, embreeErrorFunc, nullptr);
	RTCBVH bvh = rtcNewBVH(device);

	RTCBuildArguments arguments = rtcDefaultBuildArguments();
	arguments.byteSize = sizeof(arguments);
	arguments.buildFlags = RTC_BUILD_FLAG_NONE;
	arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
	arguments.maxBranchingFactor = 2;
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

	auto* constructionTreeRootNode = reinterpret_cast<ConstructionInnerNode*>(rtcBuildBVH(&arguments));

	m_innerNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHNode>>((uint32_t)m_primitives.size() * 2, 16);
	m_leafNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHLeaf>>((uint32_t)m_primitives.size() * 2, 16);
	//.m_leafNodeAllocator  = new ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::LeafNode>(1,2);

	orderChildrenConstructionBVH(constructionTreeRootNode);

	Bounds rootBounds;
	rootBounds.extend(constructionTreeRootNode->childBounds[0]);
	rootBounds.extend(constructionTreeRootNode->childBounds[1]);
	auto[rootHandle, rootPtr] = collapseTreelet(constructionTreeRootNode, rootBounds);
	m_rootHandle = rootHandle;

	rtcReleaseBVH(bvh);
	rtcReleaseDevice(device);

	std::cout << "Actual prim count: " << m_primitives.size() << std::endl;
	m_primitives.clear();
	m_primitives.shrink_to_fit();

	testBVH();
}

template <typename LeafObj>
inline void* WiVeBVH8Build2<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
	assert(numChildren == 2);

	auto* self = reinterpret_cast<WiVeBVH8<LeafObj>*>(userPtr);
	void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode), std::alignment_of_v<ConstructionInnerNode>);
	return reinterpret_cast<void*>(new (ptr) ConstructionInnerNode);
}

template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
	assert(numChildren == 2);

	auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
	for (unsigned childID = 0; childID < numChildren; childID++) {
		auto* child = reinterpret_cast<ConstructionBVHNode*>(childPtr[childID]);
		node->children[childID] = child;
	}
}

template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
	assert(numChildren == 2);

	auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
	for (unsigned childID = 0; childID < numChildren; childID++) {
		const RTCBounds* embreeBounds = bounds[childID];
		node->childBounds[childID] = Bounds(
			glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
			glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
	}

	glm::vec3 overlap = glm::min(node->childBounds[0].max, node->childBounds[1].max) - glm::max(node->childBounds[0].min, node->childBounds[1].min);
	node->splitAxis = minDimension(overlap);
}

template <typename LeafObj>
inline void* WiVeBVH8Build2<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
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

template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::orderChildrenConstructionBVH(ConstructionBVHNode* p)
{
	if (auto nodePtr = dynamic_cast<ConstructionInnerNode*>(p)) {
		if (nodePtr->childBounds[0].min[nodePtr->splitAxis] > nodePtr->childBounds[1].min[nodePtr->splitAxis]) {
			// Switch children
			std::swap(nodePtr->childBounds[0], nodePtr->childBounds[1]);
			std::swap(nodePtr->children[0], nodePtr->children[1]);
		}
		orderChildrenConstructionBVH(nodePtr->children[0]);
		orderChildrenConstructionBVH(nodePtr->children[1]);
	}
}

template <typename LeafObj>
inline std::pair<uint32_t, const typename WiVeBVH8<LeafObj>::BVHNode*> WiVeBVH8Build2<LeafObj>::collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds)
{
	struct Child {
		Child(const ConstructionInnerNode* n, int parentIndexInTreeletCopy, int childIndexInParent, const Bounds& bounds)
			: node(n)
			, parentIndexInTreeletCopy(parentIndexInTreeletCopy)
			, childIndexInParent(childIndexInParent)
			, surfaceArea(bounds.surfaceArea())
		{
		}
		inline bool operator<(const Child& other) const { return surfaceArea < other.surfaceArea; }

		const ConstructionInnerNode* node;
		int parentIndexInTreeletCopy;
		int childIndexInParent;
		float surfaceArea;
	};

	// Compute the treelet by traversing into the children with the largest surface area
	eastl::fixed_set<Child, 8> traversableTreeletLeafs; // Only contains inner nodes (actual leaf nodes (with primitives) cant be traversed)
	eastl::fixed_vector<ConstructionInnerNode, 7> treeletCopy; // 8 children -> (n - 1) = 7 inner nodes

															   // Insert root node into the treelet
	treeletCopy.push_back(*treeletRoot);
	for (int i = 0; i < 2; i++) {
		if (const auto* child = dynamic_cast<const ConstructionInnerNode*>(treeletRoot->children[i])) {
			const Bounds& bounds = treeletRoot->childBounds[i];
			traversableTreeletLeafs.insert(Child(child, 0, i, bounds));
		}
	}

	while (!(treeletCopy.full() || traversableTreeletLeafs.empty())) {
		// Traverse the treelets intermediate leaf with the largest surface area
		auto largestLeafIter = --traversableTreeletLeafs.end();
		traversableTreeletLeafs.erase(largestLeafIter);
		Child leafToTraverse = *largestLeafIter;

		// It becomes an inner node of the treelet -> create a copy in the treelet copy
		int copiedNodeIndex = (int)treeletCopy.size();
		treeletCopy.push_back(*(leafToTraverse.node));
		ConstructionInnerNode* largestLeafCopyPtr = treeletCopy.data() + copiedNodeIndex;

		// Update the child pointer of the [copied] parent node
		ConstructionInnerNode& parent = treeletCopy[leafToTraverse.parentIndexInTreeletCopy];
		parent.children[leafToTraverse.childIndexInParent] = largestLeafCopyPtr;

		// Add the nodes children to the list of treelet intermediate leafs
		for (int i = 0; i < 2; i++) {
			if (const auto* child = dynamic_cast<const ConstructionInnerNode*>(leafToTraverse.node->children[i])) {
				const Bounds& bounds = leafToTraverse.node->childBounds[i];
				traversableTreeletLeafs.insert(Child(child, copiedNodeIndex, i, bounds));
			}
			// Leaf nodes cannot be traversed so ignore those children (they become leafs of the final treelet)
		}
	}

	//std::cout << "Collapse - " << treeletCopy.size() << " children" << std::endl;

	// Convert the treelet into an 8 wide BVH node
	int treeletLeafsFound = 0;
	std::array<float, 8> minX, maxX, minY, maxY, minZ, maxZ;
	std::array<uint32_t, 8> permOffsetsAndFlags, children;
	eastl::fixed_map<const ConstructionBVHNode*, uint32_t, 32> leafToChildIndexMapping;

	auto isTreeletNode = [&](const ConstructionInnerNode* const n) { return n >= treeletCopy.data() && n < treeletCopy.data() + treeletCopy.size(); };
	eastl::fixed_vector<const ConstructionInnerNode*, 7> stack = { &treeletCopy[0] };
	while (!stack.empty()) {
		const auto* nodePtr = stack.back();
		stack.pop_back();

		for (int childID = 0; childID < 2; childID++) {
			bool isIntermediateLeaf;
			bool isLeaf;
			uint32_t numLeafObjects = 0;
			if (auto childPtr = dynamic_cast<const ConstructionLeafNode*>(nodePtr->children[childID])) {
				// An actual leaf node -> also a treelet intermediate leaf node
				isIntermediateLeaf = true;
				isLeaf = true;
				numLeafObjects = static_cast<uint32_t>(childPtr->leafs.size());
			} else if (auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[childID]); !isTreeletNode(childPtr)) {
				// Treelet intermediate leaf node that is an inner node (of the whole tree)
				isIntermediateLeaf = true;
				isLeaf = false;
			} else {
				// Treelet inner node
				isIntermediateLeaf = false;
				isLeaf = false;
			}

			if (isIntermediateLeaf) {
				const Bounds& bounds = nodePtr->childBounds[childID];
				minX[treeletLeafsFound] = bounds.min.x;
				minY[treeletLeafsFound] = bounds.min.y;
				minZ[treeletLeafsFound] = bounds.min.z;
				maxX[treeletLeafsFound] = bounds.max.x;
				maxY[treeletLeafsFound] = bounds.max.y;
				maxZ[treeletLeafsFound] = bounds.max.z;

				if (isLeaf) {
					// Allocate primitive node
					auto[leafHandle, leafPtr] = m_leafNodeAllocator->allocate();
					auto constructionLeafPtr = dynamic_cast<const ConstructionLeafNode*>(nodePtr->children[childID]);
					for (size_t i = 0; i < constructionLeafPtr->leafs.size(); i++) {
						leafPtr->leafObjectIDs[i] = std::get<0>(constructionLeafPtr->leafs[i]);
						leafPtr->primitiveIDs[i] = std::get<1>(constructionLeafPtr->leafs[i]);
					}
					for (size_t i = constructionLeafPtr->leafs.size(); i < 4; i++) {
						leafPtr->leafObjectIDs[i] = emptyHandle;
					}
					permOffsetsAndFlags[treeletLeafsFound] = createFlagsLeaf();
					children[treeletLeafsFound] = leafHandle;
					leafToChildIndexMapping[nodePtr->children[childID]] = treeletLeafsFound;
				} else {
					// Recurse to get the child index in the final BVH
					permOffsetsAndFlags[treeletLeafsFound] = createFlagsInner();
					auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[childID]);
					auto[intermediateLeafHandle, _] = collapseTreelet(childPtr, bounds);
					children[treeletLeafsFound] = intermediateLeafHandle;
					leafToChildIndexMapping[nodePtr->children[childID]] = treeletLeafsFound;
				}

				treeletLeafsFound++;
			} else {
				auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[childID]);
				stack.push_back(childPtr);
			}
		}
	}
	for (int i = treeletLeafsFound; i < 8; i++) {
		permOffsetsAndFlags[i] = createFlagsEmpty();
		minX[i] = 0.0f;
		minY[i] = 0.0f;
		minZ[i] = 0.0f;
		maxX[i] = 0.0f;
		maxY[i] = 0.0f;
		maxZ[i] = 0.0f;
	}

	// Compute permutation offsets
	for (int x = -1; x <= 1; x += 2) {
		for (int y = -1; y <= 1; y += 2) {
			for (int z = -1; z <= 1; z += 2) {
				bool flipX = x < 0;
				bool flipY = y < 0;
				bool flipZ = z < 0;

				eastl::fixed_vector<uint32_t, 8> permutationOffsets;

				stack.clear();
				stack.push_back(&treeletCopy[0]);
				while (!stack.empty()) {
					const auto* nodePtr = stack.back();
					stack.pop_back();

					if ((nodePtr->splitAxis == 0 && flipX) || (nodePtr->splitAxis == 1 && flipY) || (nodePtr->splitAxis == 2 && flipZ)) {
						if (auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[1]); isTreeletNode(childPtr))
							stack.push_back(childPtr);
						else {
							assert(leafToChildIndexMapping.find(nodePtr->children[1]) != leafToChildIndexMapping.end());
							permutationOffsets.push_back(leafToChildIndexMapping[nodePtr->children[1]]);
						}

						if (auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[0]); isTreeletNode(childPtr))
							stack.push_back(childPtr);
						else {
							assert(leafToChildIndexMapping.find(nodePtr->children[0]) != leafToChildIndexMapping.end());
							permutationOffsets.push_back(leafToChildIndexMapping[nodePtr->children[0]]);
						}
					} else {
						if (auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[0]); isTreeletNode(childPtr))
							stack.push_back(childPtr);
						else {
							assert(leafToChildIndexMapping.find(nodePtr->children[0]) != leafToChildIndexMapping.end());
							permutationOffsets.push_back(leafToChildIndexMapping[nodePtr->children[0]]);
						}

						if (auto childPtr = dynamic_cast<const ConstructionInnerNode*>(nodePtr->children[1]); isTreeletNode(childPtr))
							stack.push_back(childPtr);
						else {
							assert(leafToChildIndexMapping.find(nodePtr->children[1]) != leafToChildIndexMapping.end());
							permutationOffsets.push_back(leafToChildIndexMapping[nodePtr->children[1]]);
						}
					}
				}

				assert(permutationOffsets.size() == (size_t)treeletLeafsFound);
				uint32_t shiftAmount = signShiftAmount(x > 0, y > 0, z > 0);
				assert(shiftAmount <= (24 - 3));
				for (int i = 0; i < treeletLeafsFound; i++) {
					assert((int)permutationOffsets[i] <= treeletLeafsFound);
					permOffsetsAndFlags[i] |= (permutationOffsets[i] & 0b111) << shiftAmount;
				}

				for (int i = treeletLeafsFound; i < 8; i++) {
					permOffsetsAndFlags[i] |= (i & 0b111) << shiftAmount;
				}
			}
		}
	}

	// Allocate and fill BVH8 node
	auto[bvh8NodeHandle, bvh8NodePtr] = m_innerNodeAllocator->allocate();
	bvh8NodePtr->minX.load(minX);
	bvh8NodePtr->minY.load(minY);
	bvh8NodePtr->minZ.load(minZ);
	bvh8NodePtr->maxX.load(maxX);
	bvh8NodePtr->maxY.load(maxY);
	bvh8NodePtr->maxZ.load(maxZ);
	bvh8NodePtr->permOffsetsAndFlags.load(permOffsetsAndFlags);
	bvh8NodePtr->children.load(children);
	return { bvh8NodeHandle, bvh8NodePtr };
}
