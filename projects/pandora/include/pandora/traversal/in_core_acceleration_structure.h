#pragma once
#include "pandora/core/scene.h"
#include "pandora/core/stats.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/embree_bvh.h"
#include "pandora/traversal/bvh/naive_single_bvh2.h"
#include "pandora/traversal/bvh/wive_bvh8_build2.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/memory_arena_ts.h"
#include <gsl/gsl>
#include <memory>
#include <unordered_map>
#include <unordered_set>

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
    template <typename T>
    using BVHType = WiVeBVH8Build8<T>;

    // Leaf node of a bottom-level (instanced) BVH
    class InstanceLeafNode {
    public:
        InstanceLeafNode(const TriangleMesh& triangleMesh, unsigned primitiveID);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        // Is a primitive
        const TriangleMesh& m_triangleMesh;
        const unsigned m_primitiveID;
    };

    class LeafNode {
    public:
        LeafNode(const SceneObject& sceneObject, const std::shared_ptr<BVHType<InstanceLeafNode>>& bvh); // Instanced object in the top-level BVH
        LeafNode(const SceneObject& sceneObject, unsigned primitiveID); // Leaf of the top-level BVH

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        // Is a primitive
        const SceneObject& m_sceneObject;
        const unsigned m_primitiveID;

        // Is an instance
        std::shared_ptr<BVHType<InstanceLeafNode>> m_bvh;
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

    static BVHType<LeafNode> buildBVH(gsl::span<const std::unique_ptr<SceneObject>>);

    static PauseableBVH4<PauseableLeafNode, UserState> buildPauseableBVH(gsl::span<const std::unique_ptr<SceneObject>>);

private:
    BVHType<LeafNode> m_bvh;
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
    : m_bvh(std::move(buildBVH(sceneObjects)))
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
inline InCoreAccelerationStructure<UserState>::BVHType<typename InCoreAccelerationStructure<UserState>::LeafNode> InCoreAccelerationStructure<UserState>::buildBVH(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects)
{
    std::unordered_set<const TriangleMesh*> instancedMeshes;
    for (const auto& sceneObject : sceneObjects) {
        if (sceneObject->hasTransform()) {
            instancedMeshes.insert(sceneObject->getMesh().get());
        }
    }

    std::unordered_map<const TriangleMesh*, std::shared_ptr<BVHType<InstanceLeafNode>>> instancedBVHs;
    for (auto meshPtr : instancedMeshes) {
        // The triangle mesh has multiple occurences in the tree. Build a single BVH for the triangle mesh and repeat that
        //  BVH multiple times throughout the scene.
        std::vector<InstanceLeafNode> instanceTemplateLeafs;
        for (unsigned primitiveID = 0; primitiveID < meshPtr->numTriangles(); primitiveID++) {
            instanceTemplateLeafs.push_back(InstanceLeafNode(*meshPtr, primitiveID));
        }

        auto bvh = std::make_shared<BVHType<InstanceLeafNode>>();
        bvh->build(instanceTemplateLeafs);
        instancedBVHs[meshPtr] = bvh;
    }

    std::vector<LeafNode> leafs;
    for (const auto& sceneObject : sceneObjects) {
        const auto& mesh = *sceneObject->getMesh();
        if (sceneObject->hasTransform()) {
            std::shared_ptr<BVHType<InstanceLeafNode>> bvh = instancedBVHs[sceneObject->getMesh().get()];
            leafs.emplace_back(*sceneObject, bvh);
        } else {
            // Single occurence => primitives can be incorporated into the top level tree for better performance
            for (unsigned primitiveID = 0; primitiveID < mesh.numTriangles(); primitiveID++) {
                leafs.emplace_back(*sceneObject, primitiveID);
            }
        }
    }

    BVHType<LeafNode> bvh;
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
inline InCoreAccelerationStructure<UserState>::InstanceLeafNode::InstanceLeafNode(const TriangleMesh& triangleMesh, unsigned primitiveID)
    : m_triangleMesh(triangleMesh)
    , m_primitiveID(primitiveID)
{
}

template <typename UserState>
inline Bounds InCoreAccelerationStructure<UserState>::InstanceLeafNode::getBounds() const
{
    return m_triangleMesh.getPrimitiveBounds(m_primitiveID);
}

template <typename UserState>
inline bool InCoreAccelerationStructure<UserState>::InstanceLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    bool hit = m_triangleMesh.intersectPrimitive(ray, hitInfo, m_primitiveID);
    if (hit)
        hitInfo.primitiveID = m_primitiveID;
    return hit;
}

template <typename UserState>
inline InCoreAccelerationStructure<UserState>::LeafNode::LeafNode(const SceneObject& sceneObject, const std::shared_ptr<BVHType<InstanceLeafNode>>& bvh)
    : m_sceneObject(sceneObject)
    , m_primitiveID(-1)
    , m_bvh(bvh)
{
}

template <typename UserState>
inline InCoreAccelerationStructure<UserState>::LeafNode::LeafNode(const SceneObject& sceneObject, unsigned primitiveID)
    : m_sceneObject(sceneObject)
    , m_primitiveID(primitiveID)
    , m_bvh(nullptr)
{
}

template <typename UserState>
inline Bounds InCoreAccelerationStructure<UserState>::LeafNode::getBounds() const
{
    if (m_bvh) {
        Bounds bounds = m_sceneObject.getMeshRef().getBounds();
        bounds.transform(m_sceneObject.getInstanceToWorldTransform());
        return bounds;
    } else {
        return m_sceneObject.getMeshRef().getPrimitiveBounds(m_primitiveID);
    }
}

template <typename UserState>
inline bool InCoreAccelerationStructure<UserState>::LeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    if (!m_bvh) {
        bool hit = m_sceneObject.getMeshRef().intersectPrimitive(ray, hitInfo, m_primitiveID);
        if (hit) {
            hitInfo.sceneObject = &m_sceneObject;
            hitInfo.primitiveID = m_primitiveID;
        }
        return hit;
    } else {
        glm::vec3 origin = ray.origin;
        glm::vec3 direction = ray.direction;
        
        glm::mat4 transform = m_sceneObject.getWorldToInstanceTransform();
        ray.origin = transform * glm::vec4(ray.origin, 1.0f);
        ray.direction = transform * glm::vec4(ray.direction, 0.0f);

        bool hit = m_bvh->intersect(ray, hitInfo);
        if (hit) {
            hitInfo.sceneObject = &m_sceneObject;
        }

        ray.origin = origin;
        ray.direction = direction;

        return hit;
    }
}
}
