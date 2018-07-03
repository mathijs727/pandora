#include <EASTL/fixed_map.h>
#include <EASTL/fixed_set.h>
#include <EASTL/fixed_vector.h>
#include <algorithm>
#include <array>
#include <cassert>

namespace pandora {

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
