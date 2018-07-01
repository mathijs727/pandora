#include <EASTL/fixed_set.h>
#include <EASTL/fixed_vector.h>

namespace pandora {

template <typename LeafObj>
inline WiveBVH8<LeafObj>::WiveBVH8()
    : m_root(nullptr)
{
}

template <typename LeafObj>
inline WiveBVH8<LeafObj>::~WiveBVH8()
{
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::addObject(const LeafObj* addObject)
{
    for (unsigned primitiveID = 0; primitiveID < addObject->numPrimitives(); primitiveID++) {
        auto bounds = addObject->getPrimitiveBounds(primitiveID);

        RTCBuildPrimitive primitive;
        primitive.lower_x = bounds.min.x;
        primitive.lower_y = bounds.min.y;
        primitive.lower_z = bounds.min.z;
        primitive.upper_x = bounds.max.x;
        primitive.upper_y = bounds.max.y;
        primitive.upper_z = bounds.max.z;
        primitive.primID = primitiveID;
        primitive.geomID = (unsigned)m_leafObjects.size();

        m_primitives.push_back(primitive);
        m_leafObjects.push_back(addObject); // Vector of references is a nightmare
    }
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::commit()
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

    m_root = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

    m_primitives.clear();
    m_primitives.shrink_to_fit();
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    return false;
}

template <typename LeafObj>
inline void* WiveBVH8<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);

    auto* self = reinterpret_cast<WiveBVH8<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode), std::alignment_of_v<ConstructionInnerNode>);
    return reinterpret_cast<void*>(new (ptr) ConstructionInnerNode);
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);

    auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        auto* child = reinterpret_cast<const ConstructionBVHNode*>(childPtr[childID]);
        node->children[childID] = child;
    }
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);

    auto* node = reinterpret_cast<ConstructionInnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        const RTCBounds* embreeBounds = bounds[childID];
        node->childBounds[childID] = Bounds(
            glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
            glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
    }
}

template <typename LeafObj>
inline void* WiveBVH8<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims <= 4);

    auto* self = reinterpret_cast<WiveBVH8<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionLeafNode), std::alignment_of_v<ConstructionLeafNode>);

    ConstructionLeafNode* leafNode = new (ptr) ConstructionLeafNode();
    for (size_t i = 0; i < numPrims; i++) {
        leafNode->leafs.push_back({ prims[i].geomID, prims[i].primID });
    }
    return ptr;
}

template <typename LeafObj>
inline typename WiveBVH8<LeafObj>::BVHNode* WiveBVH8<LeafObj>::collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds)
{
    struct Child {
        Child(const ConstructionInnerNode* n, int parentIndexInTreeletCopy, int childIndexInParent, const Bounds& bounds)
            : node(n)
            , parentIndexInTreeletCopy(parentIndexInTreeletCopy)
            , childIndexInParent(childIndexInParent)
            , surfaceArea(bounds.surfaceArea())
        {
        }
        inline operator<(const Child& other) const { return surfaceArea < other.surfaceArea; }

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
        if (const auto* child = dynamic_cast<ConstructionInnerNode>(treeletRoot->children[i])) {
            const Bounds& bounds = treeletRoot->childBounds[i];
            traversableTreeletLeafs.push_back(Child(child, 0, i, bounds));
        }
    }

    while (!(treeletCopy.full() || treeletLeafs.empty())) {
        // Traverse the treelets intermediate leaf with the largest surface area
        auto largestLeafIter = traversableTreeletLeafs.end() - 1;
        traversableTreeletLeafs.erase(largestLeafIter);
        Child leafToTraverse = *largestLeafIter;

        // It becomes an inner node of the treelet -> create a copy in the treelet copy
        int copiedNodeIndex = treeletCopy.size();
        treeletCopy.push_back(*(leafToTraverse.node));
        const ConstructionInnerNode* largestLeafCopyPtr = treeletCopy.data() + copiedNodeIndex;

        // Update the child pointer of the [copied] parent node
        ConstructionInnerNode& parent = treeletCopy[leafToTraverse.parentIndexInTreeletCopy];
        parent.children[leafToTraverse.childIndexInParent] = largestLeafCopyPtr;

        // Add the nodes children to the list of treelet intermediate leafs
        for (int i = 0; i < 2; i++) {
            if (const auto* child = dynamic_cast<ConstructionInnerNode>(leafToTraverse.node->children[i])) {
                const Bounds& bounds = leafToTraverse.node->childBounds[i];
                traversableTreeletLeafs.push_back(Child(child, copiedNodeIndex, i, bounds));
            }
            // Leaf nodes cannot be traversed so ignore those children (they become leafs of the final treelet)
        }
    }

    // Convert the treelet into an 8 wide BVH node
	float minX[8], maxX[8], minY[8], maxY[8], minZ[8], maxZ[8];
	int permOffsetsAndFlags[8], children[8];

    auto isTreeletNode(const ConstructionInnerNode* n) { return n >= treeletCopy.data() && n < treeletCopy.data() + treeletCopy.size(); };
    std::fixed_vector<const ConstructionInnerNode*, 7> stack = { treeletCopy[0] };
    while (!stack.empty()) {
        const auto* node = stack.back();
        stack.pop_back();


    }
    return nullptr;
}
}
