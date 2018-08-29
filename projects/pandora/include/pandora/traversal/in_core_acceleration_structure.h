#pragma once
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/embree_bvh.h"
#include "pandora/traversal/bvh/naive_single_bvh2.h"
//#include "pandora/traversal/bvh/wive_bvh8_build2.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/memory_arena_ts.h"
#include "pandora/core/stats.h"
#include <gsl/gsl>
#include <memory>

namespace pandora {

template <typename UserState>
class InCoreAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
    ~InCoreAccelerationStructure() = default;

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);
    void placeIntersectAnyRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

    void flush() {}; // Dummy
private:
    class LeafNode {
    public:
        unsigned numPrimitives() const;
        Bounds getPrimitiveBounds(unsigned primitiveID) const;
        //inline bool intersectPrimitive(Ray& ray, SurfaceInteraction& si, unsigned primitiveID) const;
        inline bool intersectPrimitive(Ray& ray, RayHit& hitInfo, unsigned primitiveID) const;
    };

    class PauseableLeafNode {
    public:
        PauseableLeafNode() = default;
        PauseableLeafNode(const SceneObject* sceneObject, uint32_t primitiveID)
            : sceneObject(sceneObject)
            , primitiveID(primitiveID)
        {
        }
        bool intersect(Ray& ray, RayHit& hitInfo, const UserState& userState, PauseableBVHInsertHandle handle) const;

    private:
        const SceneObject* sceneObject;
        uint32_t primitiveID;
    };

    template <typename T>
    static T buildBVH(gsl::span<const std::unique_ptr<SceneObject>>);

    static PauseableBVH4<PauseableLeafNode, UserState> buildPauseableBVH(gsl::span<const std::unique_ptr<SceneObject>>);

private:
    EmbreeBVH<LeafNode> m_bvh;
    //NaiveSingleRayBVH2<LeafNode> m_bvh;
    //WiVeBVH8Build8<LeafNode> m_bvh;
    //PauseableBVH4<PauseableLeafNode, UserState> m_bvh;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState>
inline PauseableBVH4<typename InCoreAccelerationStructure<UserState>::PauseableLeafNode, UserState> InCoreAccelerationStructure<UserState>::buildPauseableBVH(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects)
{
    std::vector<PauseableLeafNode> leafs;
    std::vector<Bounds> bounds;
    for (const auto& sceneObject : sceneObjects) {
        unsigned numPrimitives = sceneObject->getMeshRef().numTriangles();
        for (uint32_t primitiveID = 0; primitiveID < numPrimitives; primitiveID++) {
            leafs.emplace_back(sceneObject.get(), primitiveID);
            bounds.emplace_back(sceneObject->getMeshRef().getPrimitiveBounds(primitiveID));
        }
    }

    return PauseableBVH4<PauseableLeafNode, UserState>(leafs, bounds);
}

template <typename UserState>
inline bool InCoreAccelerationStructure<UserState>::PauseableLeafNode::intersect(Ray& ray, RayHit& hitInfo, const UserState& state, PauseableBVHInsertHandle insertHandle) const
{
    (void)state;
    (void)insertHandle;

    bool hit = sceneObject->getMeshRef().intersectPrimitive(ray, hitInfo, primitiveID);
    if (hit) {
        hitInfo.sceneObject = sceneObject;
        hitInfo.primitiveID = primitiveID;
    }
    return true; // Continue traversal (don't pause)
}

template <typename UserState>
inline InCoreAccelerationStructure<UserState>::InCoreAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_bvh(std::move(buildBVH<decltype(m_bvh)>(sceneObjects)))
    //: m_bvh(std::move(buildPauseableBVH(sceneObjects)))
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
    //m_bvh.saveToFile("scene.bvh");
    //m_bvh.loadFromFile("scene.bvh", leafs);

    g_stats.memory.botBVH += m_bvh.size();
}

template <typename UserState>
template <typename T>
inline T InCoreAccelerationStructure<UserState>::buildBVH(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects)
{
    std::vector<const LeafNode*> leafs;
    for (const auto& sceneObject : sceneObjects) {
        // Reinterpret sceneObject pointer as LeafNode pointer. This allows us to remove an unnecessary indirection (BVH -> LeafNode -> SceneObject becomes BVH -> SceneObject).
        leafs.push_back(reinterpret_cast<const LeafNode*>(sceneObject.get()));
    }

    T bvh;
    bvh.build(leafs);
    return std::move(bvh);
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
        RayHit hitInfo = RayHit();
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        bool paused;
        if constexpr (std::is_same_v<decltype(m_bvh), PauseableBVH4<PauseableLeafNode, UserState>>) {
            paused = !m_bvh.intersect(ray, hitInfo, userState);
        } else {
            m_bvh.intersect(ray, hitInfo);
            paused = false;
        }
        assert(!paused);
        if (hitInfo.sceneObject) {
            SurfaceInteraction si = hitInfo.sceneObject->getMeshRef().fillSurfaceInteraction(ray, hitInfo);
            m_hitCallback(ray, si, perRayUserData[i], nullptr);
        } else {
            m_missCallback(ray, perRayUserData[i]);
        }
    }
}

template <typename UserState>
inline void InCoreAccelerationStructure<UserState>::placeIntersectAnyRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        bool hit;
        if constexpr (std::is_same_v<decltype(m_bvh), PauseableBVH4<PauseableLeafNode, UserState>>) {
            // NOTE: use regular intersection code for now. I just want this code path to keep working as-is (performance is not important).
            RayHit hitInfo = RayHit();
            bool paused = !m_bvh.intersect(ray, hitInfo, userState);
            assert(!paused);

            hit = hitInfo.sceneObject != nullptr;
        } else {
            hit = m_bvh.intersectAny(ray);
        }

        if (hit) {
            m_anyHitCallback(ray, perRayUserData[i]);
        } else {
            m_missCallback(ray, perRayUserData[i]);
        }
    }
}

template <typename UserState>
inline unsigned InCoreAccelerationStructure<UserState>::LeafNode::numPrimitives() const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().numTriangles();
}

template <typename UserState>
inline Bounds InCoreAccelerationStructure<UserState>::LeafNode::getPrimitiveBounds(unsigned primitiveID) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().getPrimitiveBounds(primitiveID);
}

template <typename UserState>
inline bool InCoreAccelerationStructure<UserState>::LeafNode::intersectPrimitive(Ray& ray, RayHit& hitInfo, unsigned primitiveID) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);

    bool hit = sceneObject->getMeshRef().intersectPrimitive(ray, hitInfo, primitiveID);
    if (hit) {
        hitInfo.sceneObject = sceneObject;
        hitInfo.primitiveID = primitiveID;
    }
    return hit;
}

}
