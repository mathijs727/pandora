#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include "pandora/utility/memory_arena_ts.h"
#include <embree3/rtcore.h>
#include <gsl/gsl>
#include <tuple>
#include <vector>

namespace pandora {

template <typename LeafObj>
class SingleRayBVH : public BVH<LeafObj> {
public:
    SingleRayBVH();
    ~SingleRayBVH();

    void addPrimitive(const LeafObj& ref) override final;
    void commit() override final;

    bool intersect(Ray& ray, SurfaceInteraction& si) const override final;

private:
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);

    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    std::vector<const LeafObj*> m_leafObjects;
    std::vector<RTCBuildPrimitive> m_primitives;
    RTCDevice m_device;
    RTCBVH m_bvh;

    struct BVHNode {
        virtual bool intersect(Ray& ray, SurfaceInteraction& si) const = 0;
    };
    struct LeafNode : public BVHNode {
        const LeafObj* leafObject;
        unsigned primitiveID;

        bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
    };
    struct InnerNode : public BVHNode {
        Bounds childBounds[2];
        const BVHNode* children[2];

        bool intersect(Ray& ray, SurfaceInteraction& si) const override final;
    };

	const BVHNode* m_root;

    /*struct BVHNode {
        Bounds bounds;
        uint32_t leftFirst; // Left child index / first child index
        uint32_t count; // Number of primitives (0 for inner nodes)
    };
    MemoryArenaTS m_bvhNodeAllocator;*/
};

template <typename LeafObj>
inline SingleRayBVH<LeafObj>::SingleRayBVH()
{
    m_device = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_device, embreeErrorFunc, nullptr);
    m_bvh = rtcNewBVH(m_device);
}

template <typename LeafObj>
inline SingleRayBVH<LeafObj>::~SingleRayBVH()
{
    rtcReleaseBVH(m_bvh);
    rtcReleaseDevice(m_device);
}

template <typename LeafObj>
inline void SingleRayBVH<LeafObj>::addPrimitive(const LeafObj& ref)
{
    for (unsigned primitiveID = 0; primitiveID < ref.numPrimitives(); primitiveID++) {
        auto bounds = ref.getPrimitiveBounds(primitiveID);

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
        m_leafObjects.push_back(&ref); // Vector of references is a nightmare
    }
}

template <typename LeafObj>
inline void SingleRayBVH<LeafObj>::commit()
{
    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
    arguments.maxBranchingFactor = 2;
    arguments.minLeafSize = 1;
    arguments.maxLeafSize = 1;
    arguments.bvh = m_bvh;
    arguments.primitives = m_primitives.data();
    arguments.primitiveCount = m_primitives.size();
    arguments.primitiveArrayCapacity = m_primitives.capacity();
    arguments.createNode = innerNodeCreate;
    arguments.setNodeChildren = innerNodeSetChildren;
    arguments.setNodeBounds = innerNodeSetBounds;
    arguments.createLeaf = leafCreate;
    arguments.userPtr = this;

	m_root = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));

    m_primitives.clear();
    m_primitives.shrink_to_fit();
}

template <typename LeafObj>
inline bool SingleRayBVH<LeafObj>::intersect(Ray& ray, SurfaceInteraction& si) const
{
    bool hit = m_root->intersect(ray, si);
	si.wo = -ray.direction;
	return hit;
}

template <typename LeafObj>
inline void* SingleRayBVH<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);

    auto* self = reinterpret_cast<SingleRayBVH<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), 16);
    return reinterpret_cast<void*>(new (ptr) InnerNode);
}

template <typename LeafObj>
inline void SingleRayBVH<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    //auto* self = reinterpret_cast<SingleRayBVH<LeafObj>*>(userPtr);
    (void)userPtr;

    auto* node = reinterpret_cast<InnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        auto* child = reinterpret_cast<const BVHNode*>(childPtr[childID]);
        node->children[childID] = child;
    }
}

template <typename LeafObj>
inline void SingleRayBVH<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    //auto* self = reinterpret_cast<SingleRayBVH<LeafObj>*>(userPtr);
    (void)userPtr;

    auto* node = reinterpret_cast<InnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        auto* embreeBounds = bounds[childID];
        node->childBounds[childID] = Bounds(
            glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
            glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
    }
}

template <typename LeafObj>
inline void* SingleRayBVH<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims == 1);

    auto* self = reinterpret_cast<SingleRayBVH<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(LeafNode), 16);

    LeafNode* leafNode = new (ptr) LeafNode();
    leafNode->leafObject = self->m_leafObjects[prims[0].geomID];
    leafNode->primitiveID = prims[0].primID;
    return ptr;
}

template <typename LeafObj>
inline bool SingleRayBVH<LeafObj>::InnerNode::intersect(Ray& ray, SurfaceInteraction& si) const
{
    float tmin0, tmax0;
    float tmin1, tmax1;
    bool hitBounds0 = childBounds[0].intersect(ray, tmin0, tmax0);
    bool hitBounds1 = childBounds[1].intersect(ray, tmin1, tmax1);

    if (hitBounds0 && hitBounds1) {
		bool hit = false;
        if (tmin0 < tmin1) {
            hit |= children[0]->intersect(ray, si);
            if (ray.tfar > tmin1)
				hit |= children[1]->intersect(ray, si);
        } else {
			hit |= children[1]->intersect(ray, si);
			if (ray.tfar > tmin0)
				hit |= children[0]->intersect(ray, si);
        }
		return hit;
    } else if (hitBounds0) {
        return children[0]->intersect(ray, si);
    } else if (hitBounds1) {
        return children[1]->intersect(ray, si);
    }
	return false;
}

template <typename LeafObj>
inline bool SingleRayBVH<LeafObj>::LeafNode::intersect(Ray& ray, SurfaceInteraction& si) const
{
    return leafObject->intersectPrimitive(primitiveID, ray, si);
}

}
