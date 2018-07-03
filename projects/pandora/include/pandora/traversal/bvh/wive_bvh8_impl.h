#include <EASTL/fixed_map.h>
#include <EASTL/fixed_set.h>
#include <EASTL/fixed_vector.h>
#include <algorithm>
#include <array>
#include <cassert>

namespace pandora {

template <typename LeafObj>
inline WiveBVH8<LeafObj>::WiveBVH8()
    : m_rootHandle(emptyHandle)
{
}

template <typename LeafObj>
inline WiveBVH8<LeafObj>::~WiveBVH8()
{
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    si.sceneObject = nullptr;
    bool hit = false;

    SIMDRay simdRay;
    simdRay.originX = simd::vec8_f32(ray.origin.x);
    simdRay.originY = simd::vec8_f32(ray.origin.y);
    simdRay.originZ = simd::vec8_f32(ray.origin.z);
    simdRay.invDirectionX = simd::vec8_f32(1.0f / ray.direction.x);
    simdRay.invDirectionY = simd::vec8_f32(1.0f / ray.direction.y);
    simdRay.invDirectionZ = simd::vec8_f32(1.0f / ray.direction.z);
    simdRay.tnear = simd::vec8_f32(ray.tnear);
    simdRay.tfar = simd::vec8_f32(ray.tfar);
    simdRay.raySignShiftAmount = simd::vec8_u32(signShiftAmount(ray.direction.x > 0, ray.direction.y > 0, ray.direction.z > 0));

    struct StackItem {
        uint32_t nodeHandle;
        float distance;
        bool isLeaf;
    };
    eastl::fixed_vector<StackItem, 10> stack;
    stack.push_back(StackItem{ m_rootHandle, 0.0f, false });
    while (!stack.empty()) {
        StackItem item = stack.back();
        stack.pop_back();

        if (ray.tfar < item.distance)
            continue;

        if (!item.isLeaf) {
            // Inner node
            simd::vec8_u32 childrenSIMD;
            simd::vec8_u32 childTypesSIMD;
            simd::vec8_f32 distancesSIMD;
            uint32_t numChildren;
            traverseCluster(&m_innerNodeAllocator->get(item.nodeHandle), simdRay, childrenSIMD, childTypesSIMD, distancesSIMD, numChildren);

            std::array<uint32_t, 8> children;
            std::array<uint32_t, 8> childTypes;
            std::array<float, 8> distances;
            childrenSIMD.store(children);
            childTypesSIMD.store(childTypes);
            distancesSIMD.store(distances);
            for (uint32_t i = 0; i < numChildren; i++) {
                //assert(childTypes[i] == NodeType::InnerNode || childTypes[i] == NodeType::LeafNode);

                bool isLeaf = (childTypes[i] == NodeType::LeafNode);
                /*if (isLeaf)
					assert(children[i] < m_leafNodeAllocator->size());
				else
					assert(children[i] < m_innerNodeAllocator->size());*/
                stack.push_back(StackItem{ children[i], distances[i], isLeaf });
            }
        } else {
            // Leaf node
            hit |= intersectLeaf(&m_leafNodeAllocator->get(item.nodeHandle), ray, si);
            simdRay.tfar.broadcast(ray.tfar);
        }
    }

    si.wo = -ray.direction;
    return hit;
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::traverseCluster(const BVHNode* n, const SIMDRay& ray, simd::vec8_u32& outChildren, simd::vec8_u32& outChildTypes, simd::vec8_f32& outDistances, uint32_t& outNumChildren) const
{
    simd::vec8_f32 tx1 = (n->minX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 tx2 = (n->maxX - ray.originX) * ray.invDirectionX;
    simd::vec8_f32 ty1 = (n->minY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 ty2 = (n->maxY - ray.originY) * ray.invDirectionY;
    simd::vec8_f32 tz1 = (n->minZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 tz2 = (n->maxZ - ray.originZ) * ray.invDirectionZ;
    simd::vec8_f32 txMin = simd::min(tx1, tx2);
    simd::vec8_f32 tyMin = simd::min(ty1, ty2);
    simd::vec8_f32 tzMin = simd::min(tz1, tz2);
    simd::vec8_f32 txMax = simd::max(tx1, tx2);
    simd::vec8_f32 tyMax = simd::max(ty1, ty2);
    simd::vec8_f32 tzMax = simd::max(tz1, tz2);
    simd::vec8_f32 tmin = simd::max(ray.tnear, simd::max(txMin, simd::max(tyMin, tzMin)));
    simd::vec8_f32 tmax = simd::min(ray.tfar, simd::min(txMax, simd::min(tyMax, tzMax)));

    /*std::array<uint32_t, 8> permsAndFlags;
	n->permOffsetsAndFlags.store(permsAndFlags);
	int maxNumChildren = 0;
	for (uint32_t i = 0; i < 8; i++) {
		if (isLeafNode(permsAndFlags[i]) || isInnerNode(permsAndFlags[i]))
			maxNumChildren++;
	}

	for (int i = 0; i < maxNumChildren; i++) {
		Bounds bounds;
		bounds.grow(glm::vec3(n->minX[i], n->minY[i], n->minZ[i]));
		bounds.grow(glm::vec3(n->maxX[i], n->maxY[i], n->maxZ[i]));
		Ray scalarRay;
		scalarRay.origin = glm::vec3(ray.originX[0], ray.originY[0], ray.originZ[0]);
		scalarRay.direction = glm::vec3(1.0f / ray.invDirectionX[0], 1.0f / ray.invDirectionY[0], 1.0f / ray.invDirectionZ[0]);
		//scalarRay.tnear = ray.tnear[0];
		//scalarRay.tfar = ray.tfar[0];
		float tminScalar, tmaxScalar;
		if (bounds.intersect(scalarRay, tminScalar, tmaxScalar))
		{
			tminScalar = std::max(tminScalar, ray.tnear[0]);
			tmaxScalar = std::min(tmaxScalar, ray.tfar[0]);
			assert(std::abs(tminScalar - tmin[i]) < 0.0001f);
			assert(std::abs(tmaxScalar - tmax[i]) < 0.0001f);
		}
	}*/

    const static simd::vec8_u32 indexMask(0b111);
    const static simd::vec8_u32 simd24(24);
    simd::vec8_u32 index = (n->permOffsetsAndFlags >> ray.raySignShiftAmount) & indexMask;

    tmin = tmin.permute(index);
    tmax = tmax.permute(index);
    simd::mask8 mask = tmin < tmax;
    outChildren = n->children.permute(index).compress(mask);
    outChildTypes = (n->permOffsetsAndFlags >> simd24).permute(index).compress(mask);
    outDistances = tmin.compress(mask);
    //assert(mask.count() <= maxNumChildren);
    if (mask.count() > 4)
        assert(mask.count() <= 8);
    outNumChildren = mask.count();
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::intersectLeaf(const BVHLeaf* n, Ray& ray, SurfaceInteraction& si) const
{
    bool hit = false;
    const auto* leafObjectIDs = n->leafObjectIDs;
    const auto* primitiveIDs = n->primitiveIDs;
    for (int i = 0; i < 4; i++) {
        if (leafObjectIDs[i] == emptyHandle)
            break;

        hit |= m_leafObjects[i]->intersectPrimitive(primitiveIDs[i], ray, si);
    }
    return hit;
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::testBVH() const
{
    TestBVHData results;
    results.numPrimitives = 0;
    for (int i = 0; i < 9; i++)
        results.numChildrenHistogram[i] = 0;
    testBVHRecurse(&m_innerNodeAllocator->get(m_rootHandle), results);

    std::cout << std::endl;
    std::cout << " <<< BVH Build results >>> " << std::endl;
    std::cout << "===========================" << std::endl;
    std::cout << "Primitives reached: " << results.numPrimitives << std::endl;
    std::cout << "\nChild count histogram:\n";
    for (int i = 0; i < 8; i++)
        std::cout << i << ": " << results.numChildrenHistogram[i] << std::endl;
    std::cout << std::endl;
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::testBVHRecurse(const BVHNode* node, TestBVHData& out) const
{
    std::array<uint32_t, 8> permOffsetsAndFlags;
    std::array<uint32_t, 8> children;
    node->permOffsetsAndFlags.store(permOffsetsAndFlags);
    node->children.store(children);

    int numChildren = 0;
    for (int i = 0; i < 8; i++) {
        if (isLeafNode(permOffsetsAndFlags[i])) {
            out.numPrimitives += leafNodeChildCount(children[i]);
        } else if (isInnerNode(permOffsetsAndFlags[i])) {
            testBVHRecurse(&m_innerNodeAllocator->get(children[i]), out);
        }

        if (!isEmptyNode(permOffsetsAndFlags[i]))
            numChildren++;
    }
    out.numChildrenHistogram[numChildren]++;
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
    commit8();
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::commit8()
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
    arguments.createNode = innerNodeCreate8;
    arguments.setNodeChildren = innerNodeSetChildren8;
    arguments.setNodeBounds = innerNodeSetBounds8;
    arguments.createLeaf = leafCreate8;
    arguments.userPtr = this;

    auto* constructionTreeRootNode = reinterpret_cast<ConstructionBVHNode8*>(rtcBuildBVH(&arguments));

    m_innerNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHNode>>((uint32_t)m_primitives.size() * 2, 16);
    m_leafNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHLeaf>>((uint32_t)m_primitives.size() * 2, 16);

    auto [rootHandle, isLeaf] = constructionToFinal8(constructionTreeRootNode);
    assert(!isLeaf);
    m_rootHandle = rootHandle;

    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

    std::cout << "Actual prim count: " << m_primitives.size() << std::endl;
    m_primitives.clear();
    m_primitives.shrink_to_fit();

    testBVH();
    //std::cout << "PRESS ANY KEY TO CONTINUE" << std::endl;
    //int i;
    //std::cin >> i;
    //(void)i;
}

template <typename LeafObj>
inline std::pair<uint32_t, bool> WiveBVH8<LeafObj>::constructionToFinal8(const ConstructionBVHNode8* p)
{
    if (const auto* nodePtr = dynamic_cast<const ConstructionInnerNode8*>(p)) {
        // Allocate node
        auto [finalNodeHandle, finalNodePtr] = m_innerNodeAllocator->allocate();
		size_t numChildren = nodePtr->children.size();

        // Copy child data
        std::array<float, 8> minX, minY, minZ, maxX, maxY, maxZ;
        std::array<uint32_t, 8> children, permOffsetsAndFlags; // Permutation offsets and child flags
        for (size_t i = 0; i < numChildren; i++) {
            minX[i] = nodePtr->childBounds[i].min.x;
            minY[i] = nodePtr->childBounds[i].min.y;
            minZ[i] = nodePtr->childBounds[i].min.z;
            maxX[i] = nodePtr->childBounds[i].max.x;
            maxY[i] = nodePtr->childBounds[i].max.y;
            maxZ[i] = nodePtr->childBounds[i].max.z;
            auto [childHandle, childIsLeaf] = constructionToFinal8(nodePtr->children[i]);
            children[i] = childHandle;
            permOffsetsAndFlags[i] = (childIsLeaf ? NodeType::LeafNode : NodeType::InnerNode) << 24;
        }
		for (size_t i = numChildren; i < 8; i++) {
            minX[i] = minY[i] = minZ[i] = maxX[i] = maxY[i] = maxZ[i] = 0.0f;
            children[i] = emptyHandle;
            permOffsetsAndFlags[i] = NodeType::EmptyNode << 24;
        }

        // Create permutations
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
					std::sort(std::begin(indices), std::end(indices), [&](uint32_t a, uint32_t b) -> bool {
						return distances[a] > distances[b];
					});

					uint32_t shiftAmount = signShiftAmount(x > 0, y > 0, z > 0);
					for (int i = 0; i < 8; i++)
						permOffsetsAndFlags[i] |= indices[i] << shiftAmount;
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
        finalNodePtr->permOffsetsAndFlags.load(permOffsetsAndFlags);
        return { finalNodeHandle, false };
    } else if (const auto* nodePtr = dynamic_cast<const ConstructionLeafNode8*>(p)) {
        auto [finalNodeHandle, finalNodePtr] = m_leafNodeAllocator->allocate();
        for (size_t i = 0; i < nodePtr->leafs.size(); i++) {
            finalNodePtr->leafObjectIDs[i] = std::get<0>(nodePtr->leafs[i]);
            finalNodePtr->primitiveIDs[i] = std::get<1>(nodePtr->leafs[i]);
        }
		for (size_t i = nodePtr->leafs.size(); i < 4; i++) {
			finalNodePtr->leafObjectIDs[i] = emptyHandle;
			finalNodePtr->primitiveIDs[i] = emptyHandle;
		}
        return { finalNodeHandle, true };
    } else {
        THROW_ERROR("Unknown WiVe BVH construction node type!");
        return { -1, false };
    }
}

template <typename LeafObj>
inline void* WiveBVH8<LeafObj>::innerNodeCreate8(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    auto* self = reinterpret_cast<WiveBVH8<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionInnerNode8), std::alignment_of_v<ConstructionInnerNode8>);
    return reinterpret_cast<void*>(new (ptr) ConstructionInnerNode8);
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::innerNodeSetChildren8(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    auto* node = reinterpret_cast<ConstructionInnerNode8*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        auto* child = reinterpret_cast<ConstructionBVHNode8*>(childPtr[childID]);
        node->children.push_back(child);
    }
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::innerNodeSetBounds8(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren <= 8);

    auto* node = reinterpret_cast<ConstructionInnerNode8*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        const RTCBounds* embreeBounds = bounds[childID];
        node->childBounds.emplace_back(
            glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
            glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
    }
}

template <typename LeafObj>
inline void* WiveBVH8<LeafObj>::leafCreate8(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims <= 4);

    auto* self = reinterpret_cast<WiveBVH8<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(ConstructionLeafNode8), std::alignment_of_v<ConstructionLeafNode8>);

    ConstructionLeafNode8* leafNode = new (ptr) ConstructionLeafNode8();
    for (size_t i = 0; i < numPrims; i++) {
        leafNode->leafs.push_back({ prims[i].geomID, prims[i].primID });
    }
    return ptr;
}

template <typename LeafObj>
inline void WiveBVH8<LeafObj>::commit2()
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

    m_innerNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHNode>>((uint32_t)m_primitives.size() * 2, 16);
    m_leafNodeAllocator = std::make_unique<ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::BVHLeaf>>((uint32_t)m_primitives.size() * 2, 16);
    //.m_leafNodeAllocator  = new ContiguousAllocatorTS<typename WiveBVH8<LeafObj>::LeafNode>(1,2);

    orderChildrenConstructionBVH(constructionTreeRootNode);

    Bounds rootBounds;
    rootBounds.extend(constructionTreeRootNode->childBounds[0]);
    rootBounds.extend(constructionTreeRootNode->childBounds[1]);
    auto [rootHandle, rootPtr] = collapseTreelet(constructionTreeRootNode, rootBounds);
    m_rootHandle = rootHandle;

    rtcReleaseBVH(bvh);
    rtcReleaseDevice(device);

    std::cout << "Actual prim count: " << m_primitives.size() << std::endl;
    m_primitives.clear();
    m_primitives.shrink_to_fit();

    testBVH();
    //std::cout << "PRESS ANY KEY TO CONTINUE" << std::endl;
    //int i;
    //std::cin >> i;
    //(void)i;
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
        auto* child = reinterpret_cast<ConstructionBVHNode*>(childPtr[childID]);
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

    glm::vec3 overlap = glm::min(node->childBounds[0].max, node->childBounds[1].max) - glm::max(node->childBounds[0].min, node->childBounds[1].min);
    node->splitAxis = minDimension(overlap);
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
inline void WiveBVH8<LeafObj>::orderChildrenConstructionBVH(ConstructionBVHNode* p)
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
inline std::pair<uint32_t, const typename WiveBVH8<LeafObj>::BVHNode*> WiveBVH8<LeafObj>::collapseTreelet(const ConstructionInnerNode* treeletRoot, const Bounds& rootBounds)
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
                    auto [leafHandle, leafPtr] = m_leafNodeAllocator->allocate();
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
                    auto [intermediateLeafHandle, _] = collapseTreelet(childPtr, bounds);
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
    auto [bvh8NodeHandle, bvh8NodePtr] = m_innerNodeAllocator->allocate();
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

template <typename LeafObj>
inline uint32_t WiveBVH8<LeafObj>::createFlagsInner()
{
    return NodeType::InnerNode << 24;
}

template <typename LeafObj>
inline uint32_t WiveBVH8<LeafObj>::createFlagsLeaf()
{
    return NodeType::LeafNode << 24;
}

template <typename LeafObj>
inline uint32_t WiveBVH8<LeafObj>::createFlagsEmpty()
{
    return NodeType::EmptyNode << 24;
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::isLeafNode(uint32_t nodePermsAndFlags)
{
    return (nodePermsAndFlags >> 24) == NodeType::LeafNode;
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::isInnerNode(uint32_t nodePermsAndFlags)
{
    return (nodePermsAndFlags >> 24) == NodeType::InnerNode;
}

template <typename LeafObj>
inline bool WiveBVH8<LeafObj>::isEmptyNode(uint32_t nodePermsAndFlags)
{
    return (nodePermsAndFlags >> 24) == NodeType::EmptyNode;
}

template <typename LeafObj>
inline uint32_t WiveBVH8<LeafObj>::leafNodeChildCount(uint32_t nodeHandle) const
{
    const auto& node = m_leafNodeAllocator->get(nodeHandle);
    uint32_t count = 0;
    for (int i = 0; i < 4; i++)
        count += (node.leafObjectIDs[i] != emptyHandle ? 1 : 0);
    return count;
}

template <typename LeafObj>
inline uint32_t WiveBVH8<LeafObj>::signShiftAmount(bool positiveX, bool positiveY, bool positiveZ)
{
    return ((positiveX ? 0b001 : 0u) | (positiveY ? 0b010 : 0u) | (positiveZ ? 0b100 : 0u)) * 3;
}

}
