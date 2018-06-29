#pragma once
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/embree_bvh.h"
#include "pandora/traversal/bvh/single_ray_bvh.h"
#include "pandora/utility/memory_arena_ts.h"
#include <gsl/gsl>
#include <memory>

namespace pandora {

template <typename UserState>
class InCoreAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback);
    ~InCoreAccelerationStructure();

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

private:
    class LeafNode {
    public:
        unsigned numPrimitives() const;
        Bounds getPrimitiveBounds(unsigned primitiveID) const;
        inline bool intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const;
    };

private:
    EmbreeBVH<LeafNode> m_bvh;
    //SingleRayBVH<LeafNode> m_bvh;

    HitCallback m_hitCallback;
    MissCallback m_missCallback;
};

template <typename UserState>
inline InCoreAccelerationStructure<UserState>::InCoreAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback)
    : m_hitCallback(hitCallback)
    , m_missCallback(missCallback)
    , m_bvh()
{
    for (const auto& sceneObject : sceneObjects) {
        // Reinterpret sceneObject pointer as LeafNode pointer. This allows us to remove an unnecessary indirection (BVH -> LeafNode -> SceneObject becomes BVH -> SceneObject).
        m_bvh.addPrimitive(*reinterpret_cast<LeafNode*>(sceneObject.get()));
    }
    m_bvh.commit();
}

template <typename UserState>
inline InCoreAccelerationStructure<UserState>::~InCoreAccelerationStructure()
{
}

template <typename UserState>
inline void InCoreAccelerationStructure<UserState>::placeIntersectRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        SurfaceInteraction si;
        Ray ray = rays[i]; // Copy so we can mutate it

        if (m_bvh.intersect(ray, si))
            m_hitCallback(ray, si, perRayUserData[i], nullptr);
        else
            m_missCallback(ray, perRayUserData[i]);
    }
}

template <typename UserState>
inline unsigned InCoreAccelerationStructure<UserState>::LeafNode::numPrimitives() const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMesh().numTriangles();
}

template <typename UserState>
inline Bounds InCoreAccelerationStructure<UserState>::LeafNode::getPrimitiveBounds(unsigned primitiveID) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMesh().getPrimitiveBounds(primitiveID);
}

template <typename UserState>
inline bool InCoreAccelerationStructure<UserState>::LeafNode::intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);

    float tHit;
    bool hit = sceneObject->getMesh().intersectPrimitive(primitiveID, ray, tHit, si);
    if (hit) {
        si.sceneObject = sceneObject;
        si.primitiveID = primitiveID;
        ray.tfar = tHit;
    }
    return hit;
}

}
