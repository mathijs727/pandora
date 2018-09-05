
template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::commit(gsl::span<RTCBuildPrimitive> embreePrims, gsl::span<LeafObj> objects)
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
    arguments.primitives = embreePrims.data();
    arguments.primitiveCount = embreePrims.size();
    arguments.primitiveArrayCapacity = embreePrims.size();
    arguments.createNode = innerNodeCreate;
    arguments.setNodeChildren = innerNodeSetChildren;
    arguments.setNodeBounds = innerNodeSetBounds;
    arguments.createLeaf = leafCreate;
    arguments.userPtr = (void*)objects.data();

    auto* constructionRootPtr = reinterpret_cast<ConstructionBVHNode*>(rtcBuildBVH(&arguments));
    if (auto* x = dynamic_cast<ConstructionLeafNode*>(constructionRootPtr); x) {
        THROW_ERROR("Root may not be leaf node!");
    }
    auto* constructionTreeRootNode = dynamic_cast<ConstructionInnerNode*>(constructionRootPtr);

    m_innerNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::BVHNode>>((uint32_t)objects.size() * 2, 16);
    m_leafNodeAllocator = std::make_unique<ContiguousAllocatorTS<LeafObj>>((uint32_t)objects.size() * 2, 16);
    //.m_leafNodeAllocator  = new ContiguousAllocatorTS<typename WiVeBVH8<LeafObj>::LeafNode>(1,2);

    orderChildrenConstructionBVH(constructionTreeRootNode);

    Bounds rootBounds;
    rootBounds.extend(constructionTreeRootNode->childBounds[0]);
    rootBounds.extend(constructionTreeRootNode->childBounds[1]);
    auto [compressedRootHandle, rootPtr] = collapseTreelet(constructionTreeRootNode, rootBounds);
	m_compressedRootHandle = compressedRootHandle;

	// Releases Embree memory (including the temporary BVH)
    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

	// Shrink to fit BVH allocators
	m_innerNodeAllocator->compact();
	m_leafNodeAllocator->compact();

    testBVH();
}

template <typename LeafObj>
inline void* WiVeBVH8Build2<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    (void)userPtr;
        
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode), std::alignment_of_v<ConstructionInnerNode>);
    return reinterpret_cast<void*>(new (ptr) ConstructionInnerNode);
}

template <typename LeafObj>
inline void WiVeBVH8Build2<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    (void)userPtr;

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
    (void)userPtr;

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

    auto* leafs = reinterpret_cast<LeafObj*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionLeafNode), std::alignment_of_v<ConstructionLeafNode>);

    ConstructionLeafNode* leafNode = new (ptr) ConstructionLeafNode();
    for (size_t i = 0; i < numPrims; i++) {
        leafNode->leafs.emplace_back(std::move(leafs[prims[i].primID]));
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
        Child(const ConstructionInnerNode* n, const Bounds& bounds)
            : node(n)
            , surfaceArea(bounds.surfaceArea())
        {
        }
        inline bool operator<(const Child& other) const { return surfaceArea < other.surfaceArea; }

        const ConstructionInnerNode* node;
        float surfaceArea;
    };

    // Find the surface area of the smallest (7th largest) inner node of the treelet
    eastl::fixed_set<const ConstructionInnerNode*, 8> treeletInnerConstructionNodes;

    eastl::fixed_set<Child, 8> traversalSet; // Only contains inner nodes (actual leaf nodes (with primitives) cant be traversed)
    traversalSet.insert(Child{ treeletRoot, rootBounds });
    for (int i = 0; i < 7 && !traversalSet.empty(); i++) { // Find a max of 7 intermediate nodes (resulting in max 8 leafs respectively)
        // Traverse the treelets intermediate leaf with the largest surface area
        auto largestNodeIter = --traversalSet.end();
        traversalSet.erase(largestNodeIter);
        Child largestNode = *largestNodeIter;

        //surfaceAreaSmallestInnerNode = std::min(largestNode.surfaceArea, surfaceAreaSmallestInnerNode); // Should always be largestNode.surfaceArea because the nodes are ordered
        treeletInnerConstructionNodes.insert(largestNode.node);

        // Add the nodes children to the list of nodes to potentially be traversed
        for (int i = 0; i < 2; i++) {
            if (const auto* child = dynamic_cast<const ConstructionInnerNode*>(largestNode.node->children[i])) {
                const Bounds& bounds = largestNode.node->childBounds[i];
                traversalSet.insert(Child(child, bounds));
            }
            // Leaf nodes cannot be part of the treelet itself (they can be leafs of the treelet however)
        }
    }

    // Find the inner & leaf nodes of the treelet
    struct TreeletInnerNode {
        uint32_t leftChild, rightChild;
        bool leftIsTreeletLeaf, rightIsTreeletLeaf;
        int splitAxis;
    };
    struct TreeletLeafNode {
        uint32_t nodeHandle;
        Bounds bounds;
        std::optional<uint32_t> primitiveCount; // If node its representing is a ConstructionLeafNode
    };
    eastl::fixed_vector<TreeletInnerNode, 7> treeletInnerNodes; // 8 children -> (n - 1) = 7 inner nodes
    eastl::fixed_vector<TreeletLeafNode, 8> treeletLeafNodes; // 8 children
    std::function<std::pair<uint32_t, bool>(const ConstructionBVHNode*, const Bounds&)> createTreelet =
        [&](const ConstructionBVHNode* p, const Bounds& bounds) -> std::pair<uint32_t, bool> {
        if (const auto* node = dynamic_cast<const ConstructionInnerNode*>(p)) {
            if (treeletInnerConstructionNodes.find(node) != treeletInnerConstructionNodes.end()) {
                // Construction node is part of the treelet
                auto [leftChild, leftIsTreeletLeaf] = createTreelet(node->children[0], node->childBounds[0]);
                auto [rightChild, rightIsTreeletLeaf] = createTreelet(node->children[1], node->childBounds[1]);

                uint32_t treeletNodeID = (uint32_t)treeletInnerNodes.size();
                treeletInnerNodes.push_back(TreeletInnerNode{
                    leftChild, rightChild,
                    leftIsTreeletLeaf, rightIsTreeletLeaf,
                    node->splitAxis });
                return { treeletNodeID, false };
            } else {
                // Construction node is not part of the treelet
                uint32_t treeletNodeID = (uint32_t)treeletLeafNodes.size();
                treeletLeafNodes.push_back(TreeletLeafNode{
                    decompressNodeHandle(std::get<0>(collapseTreelet(node, bounds))),
                    bounds,
                    {} });
                return { treeletNodeID, true };
            }
        } else if (const auto* node = dynamic_cast<const ConstructionLeafNode*>(p)) {
            /*auto [leafHandle, leafPtr] = m_leafNodeAllocator->allocate();
            uint32_t primitiveCount = (uint32_t)node->leafs.size();
            for (uint32_t primID = 0; primID < primitiveCount; primID++) {
                leafPtr->leafObjectIDs[primID] = std::get<0>(node->leafs[primID]);
                leafPtr->primitiveIDs[primID] = std::get<1>(node->leafs[primID]);
            }*/
            auto primitiveCount = (unsigned)node->leafs.size();
            auto* mutNode = const_cast<ConstructionLeafNode*>(node);
            auto[leafHandle, leafPtr] = m_leafNodeAllocator->allocateNInitF(primitiveCount, [&](int i) -> LeafObj {
                return std::move(mutNode->leafs[i]);
            });

            uint32_t treeletNodeID = (uint32_t)treeletLeafNodes.size();
            treeletLeafNodes.push_back(TreeletLeafNode{
                leafHandle,
                bounds,
                primitiveCount });
            return { treeletNodeID, true };
        }

        THROW_ERROR("Unknown BVH construction node type!");
        return { -1, true };
    };
    auto [treeletRootNodeID, _] = createTreelet(treeletRoot, rootBounds);

    std::array<float, 8> minX, maxX, minY, maxY, minZ, maxZ;
    std::array<uint32_t, 8> children, permutationOffsets;

    // Collect the children bounds and handles
    uint32_t childIndex = 0;
    std::array<uint32_t, 8> leafIDToOrderedIndex;
    std::fill(std::begin(leafIDToOrderedIndex), std::end(leafIDToOrderedIndex), 0);
    std::function<void(uint32_t, bool)> collectChildren = [&](uint32_t nodeID, bool isLeaf) {
        if (isLeaf) {
            const auto& node = treeletLeafNodes[nodeID];
            minX[childIndex] = node.bounds.min.x;
            minY[childIndex] = node.bounds.min.y;
            minZ[childIndex] = node.bounds.min.z;
            maxX[childIndex] = node.bounds.max.x;
            maxY[childIndex] = node.bounds.max.y;
            maxZ[childIndex] = node.bounds.max.z;
            if (node.primitiveCount) {
                children[childIndex] = compressHandleLeaf(node.nodeHandle, *node.primitiveCount);
            } else {
                children[childIndex] = compressHandleInner(node.nodeHandle);
            }
            leafIDToOrderedIndex[nodeID] = childIndex;
            childIndex++;
        } else {
            const auto& node = treeletInnerNodes[nodeID];
            collectChildren(node.leftChild, node.leftIsTreeletLeaf);
            collectChildren(node.rightChild, node.rightIsTreeletLeaf);
        }
    };
    collectChildren(treeletRootNodeID, false);
    for (int i = childIndex; i < 8; i++) // Fill data for unused child slots
    {
        minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
        children[i] = compressHandleEmpty();
    }

    // Generate permutations by creating an approximate back-to-front order
    std::fill(std::begin(permutationOffsets), std::end(permutationOffsets), 0u);
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                bool posX = x > 0;
                bool posY = y > 0;
                bool posZ = z > 0;
                uint32_t shiftAmount = signShiftAmount(posX, posY, posZ);

                uint32_t outIndex = 0;
                std::function<void(uint32_t, bool)> generatePermutation = [&](uint32_t nodeID, bool isLeaf) {
                    if (isLeaf) {
                        permutationOffsets[outIndex] |= leafIDToOrderedIndex[nodeID] << shiftAmount;
                        outIndex++;
                    } else {
                        const auto& node = treeletInnerNodes[nodeID];
                        if ((node.splitAxis == 0 && posX) || (node.splitAxis == 1 && posY) || (node.splitAxis == 2 && posZ)) {
                            // Flip order
                            generatePermutation(node.rightChild, node.rightIsTreeletLeaf);
                            generatePermutation(node.leftChild, node.leftIsTreeletLeaf);
                        } else {
                            generatePermutation(node.leftChild, node.leftIsTreeletLeaf);
                            generatePermutation(node.rightChild, node.rightIsTreeletLeaf);
                        }
                    }
                };
                generatePermutation(treeletRootNodeID, false);
            }
        }
    }

    // Allocate and fill BVH8 node
    auto [bvh8NodeHandle, bvh8NodePtr] = m_innerNodeAllocator->allocate();
    bvh8NodePtr->minX.load(minX);
    bvh8NodePtr->minY.load(minY);
    bvh8NodePtr->minZ.load(minZ);
    bvh8NodePtr->maxX.load(maxX);
    bvh8NodePtr->maxY.load(maxY);
    bvh8NodePtr->maxZ.load(maxZ);
    bvh8NodePtr->permutationOffsets.load(permutationOffsets);
    bvh8NodePtr->children.load(children);
    return { compressHandleInner(bvh8NodeHandle), bvh8NodePtr };
}
