#include "pandora/geometry/bounds.h"
#include "pandora/traversal/bvh.h"
#include "pandora/utility/memory_arena_ts.h"
#include <EASTL/fixed_vector.h>
#include <embree3/rtcore.h>
#include <gsl/span>
#include <tuple>
#include <vector>

namespace pandora {

template <typename LeafObj>
class NaiveSingleRayBVH2 : public BVH<LeafObj> {
public:
    NaiveSingleRayBVH2();
    NaiveSingleRayBVH2(NaiveSingleRayBVH2<LeafObj>&& other);
    ~NaiveSingleRayBVH2();

    void build(gsl::span<const LeafObj*> objects) override final;

    bool intersect(Ray& ray, RayHit& hitInfo) const override final;

private:
    static void* innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr);
    static void innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr);
    static void innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr);

    static void* leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr);

private:
    struct BVHNode {
        virtual bool intersect(Ray& ray, RayHit& hitInfo) const = 0;
    };
    struct InnerNode : public BVHNode {
        Bounds childBounds[2];
        const BVHNode* children[2];

        bool intersect(Ray& ray, RayHit& hitInfo) const override final;
    };
    struct LeafNode : public BVHNode {
        eastl::fixed_vector<std::pair<const LeafObj*, unsigned>, 4> leafs;

        bool intersect(Ray& ray, RayHit& hitInfo) const override final;
    };

    std::vector<const LeafObj*> m_leafObjects;
    RTCDevice m_embreeDevice;
    RTCBVH m_embreeBVH;
    const BVHNode* m_root;
};

template <typename LeafObj>
inline NaiveSingleRayBVH2<LeafObj>::NaiveSingleRayBVH2()
{
    // Need to life for the entire lifetime of this class because we use the Embree provided allocator to store the BVH nodes
    m_embreeDevice = rtcNewDevice(nullptr);
    rtcSetDeviceErrorFunction(m_embreeDevice, embreeErrorFunc, nullptr);
    m_embreeBVH = rtcNewBVH(m_embreeDevice);
}

template <typename LeafObj>
inline NaiveSingleRayBVH2<LeafObj>::NaiveSingleRayBVH2(NaiveSingleRayBVH2<LeafObj>&& other)
    : m_leafObjects(std::move(m_leafObjects))
    , m_embreeDevice(std::move(other.m_embreeDevice))
    , m_embreeBVH(std::move(other.m_embreeBVH))
    , m_root(other.m_root)
{
    other.m_embreeBVH = nullptr;
    other.m_embreeDevice = nullptr;
}

template <typename LeafObj>
inline NaiveSingleRayBVH2<LeafObj>::~NaiveSingleRayBVH2()
{
    if (m_embreeBVH)
        rtcReleaseBVH(m_embreeBVH);
    if (m_embreeDevice)
        rtcReleaseDevice(m_embreeDevice);
}

template <typename LeafObj>
inline void NaiveSingleRayBVH2<LeafObj>::build(gsl::span<const LeafObj*> objects)
{

    std::vector<RTCBuildPrimitive> embreePrimitives;
    for (const auto* objectPtr : objects) {
        for (unsigned primitiveID = 0; primitiveID < objectPtr->numPrimitives(); primitiveID++) {
            auto bounds = objectPtr->getPrimitiveBounds(primitiveID);

            RTCBuildPrimitive primitive;
            primitive.lower_x = bounds.min.x;
            primitive.lower_y = bounds.min.y;
            primitive.lower_z = bounds.min.z;
            primitive.upper_x = bounds.max.x;
            primitive.upper_y = bounds.max.y;
            primitive.upper_z = bounds.max.z;
            primitive.primID = primitiveID;
            primitive.geomID = (unsigned)m_leafObjects.size();

            embreePrimitives.push_back(primitive);
            m_leafObjects.push_back(objectPtr); // Vector of references is a nightmare
        }
    }

    RTCBuildArguments arguments = rtcDefaultBuildArguments();
    arguments.byteSize = sizeof(arguments);
    arguments.buildFlags = RTC_BUILD_FLAG_NONE;
    arguments.buildQuality = RTC_BUILD_QUALITY_MEDIUM;
    arguments.maxBranchingFactor = 2;
    arguments.minLeafSize = 1;
    arguments.maxLeafSize = 4;
    arguments.bvh = m_embreeBVH;
    arguments.primitives = embreePrimitives.data();
    arguments.primitiveCount = embreePrimitives.size();
    arguments.primitiveArrayCapacity = embreePrimitives.capacity();
    arguments.createNode = innerNodeCreate;
    arguments.setNodeChildren = innerNodeSetChildren;
    arguments.setNodeBounds = innerNodeSetBounds;
    arguments.createLeaf = leafCreate;
    arguments.userPtr = this;

    m_root = reinterpret_cast<BVHNode*>(rtcBuildBVH(&arguments));
}

template <typename LeafObj>
inline bool NaiveSingleRayBVH2<LeafObj>::intersect(Ray& ray, RayHit& hitInfo) const
{
    return m_root->intersect(ray, hitInfo);
}

template <typename LeafObj>
inline void* NaiveSingleRayBVH2<LeafObj>::innerNodeCreate(RTCThreadLocalAllocator alloc, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);

    auto* self = reinterpret_cast<NaiveSingleRayBVH2<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), 16);
    return reinterpret_cast<void*>(new (ptr) InnerNode);
}

template <typename LeafObj>
inline void NaiveSingleRayBVH2<LeafObj>::innerNodeSetChildren(void* nodePtr, void** childPtr, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    //auto* self = reinterpret_cast<NaiveSingleRayBVH2<LeafObj>*>(userPtr);
    (void)userPtr;

    auto* node = reinterpret_cast<InnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        auto* child = reinterpret_cast<const BVHNode*>(childPtr[childID]);
        node->children[childID] = child;
    }
}

template <typename LeafObj>
inline void NaiveSingleRayBVH2<LeafObj>::innerNodeSetBounds(void* nodePtr, const RTCBounds** bounds, unsigned numChildren, void* userPtr)
{
    assert(numChildren == 2);
    //auto* self = reinterpret_cast<NaiveSingleRayBVH2<LeafObj>*>(userPtr);
    (void)userPtr;

    auto* node = reinterpret_cast<InnerNode*>(nodePtr);
    for (unsigned childID = 0; childID < numChildren; childID++) {
        const RTCBounds* embreeBounds = bounds[childID];
        node->childBounds[childID] = Bounds(
            glm::vec3(embreeBounds->lower_x, embreeBounds->lower_y, embreeBounds->lower_z),
            glm::vec3(embreeBounds->upper_x, embreeBounds->upper_y, embreeBounds->upper_z));
    }
}

template <typename LeafObj>
inline void* NaiveSingleRayBVH2<LeafObj>::leafCreate(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive* prims, size_t numPrims, void* userPtr)
{
    assert(numPrims <= 4);

    auto* self = reinterpret_cast<NaiveSingleRayBVH2<LeafObj>*>(userPtr);
    void* ptr = rtcThreadLocalAlloc(alloc, sizeof(LeafNode), 16);

    LeafNode* leafNode = new (ptr) LeafNode();
    for (size_t i = 0; i < numPrims; i++) {
        leafNode->leafs.push_back({ self->m_leafObjects[prims[i].geomID], prims[i].primID });
    }
    return ptr;
}

template <typename LeafObj>
inline bool NaiveSingleRayBVH2<LeafObj>::InnerNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    float tmin0, tmax0;
    float tmin1, tmax1;
    bool hitBounds0 = childBounds[0].intersect(ray, tmin0, tmax0);
    bool hitBounds1 = childBounds[1].intersect(ray, tmin1, tmax1);

    if (hitBounds0 && hitBounds1) {
        bool hit = false;
        if (tmin0 < tmin1) {
            hit |= children[0]->intersect(ray, hitData);
            if (ray.tfar > tmin1)
                hit |= children[1]->intersect(ray, hitData);
        } else {
            hit |= children[1]->intersect(ray, hitData);
            if (ray.tfar > tmin0)
                hit |= children[0]->intersect(ray, hitData);
        }
        return hit;
    } else if (hitBounds0) {
        return children[0]->intersect(ray, hitData);
    } else if (hitBounds1) {
        return children[1]->intersect(ray, hitData);
    }
    return false;
}

template <typename LeafObj>
inline bool NaiveSingleRayBVH2<LeafObj>::LeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    bool hit = false;
    for (auto& [leafObject, primitiveID] : leafs) {
        hit |= leafObject->intersectPrimitive(ray, hitInfo, primitiveID);
    }
    return hit;
}

}
