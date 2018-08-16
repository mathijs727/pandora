#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/memory_arena_ts.h"
#include <array>
#include <gsl/gsl>
#include <memory>

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 4096;

template <typename UserState>
class InCoreBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback);
    ~InCoreBatchingAccelerationStructure() = default;

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

private:
    static constexpr unsigned PRIMITIVES_PER_LEAF = IN_CORE_BATCHING_PRIMS_PER_LEAF;

    class BotLevelLeafNode {
    public:
        unsigned numPrimitives() const;
        Bounds getPrimitiveBounds(unsigned primitiveID) const;
        inline bool intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const;
    };

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode() = default;
        TopLevelLeafNode(gsl::span<const SceneObject*> sceneObjects);
        TopLevelLeafNode(TopLevelLeafNode&& other) = default;
        TopLevelLeafNode& operator=(TopLevelLeafNode&& other) = default;

        bool intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle insertHandle) const;

    private:
        static WiVeBVH8Build8<BotLevelLeafNode> buildBVH(gsl::span<const SceneObject*> sceneObject);

    private:
        WiVeBVH8Build8<BotLevelLeafNode> m_leafBVH;
    };

    static PauseableBVH4<TopLevelLeafNode> buildBVH(gsl::span<const std::unique_ptr<SceneObject>>);

private:
    /*template <size_t Size>
	class RayBatch {
	public:
		RayBatch() = default;
		PauseableLeafNode(const SceneObject* sceneObject, uint32_t primitiveID)
			: sceneObject(sceneObject)
			, primitiveID(primitiveID)
		{
		}
		bool intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle handle) const;

	private:
		const SceneObject* sceneObject;
		std::array<Ray, Size>;
		std::array<SurfaceInteraction, Size>;// TODO: only keep track of this for rays that have already hit something
	};*/

    PauseableBVH4<TopLevelLeafNode> m_bvh;

    HitCallback m_hitCallback;
    MissCallback m_missCallback;
};

template <typename UserState>
inline InCoreBatchingAccelerationStructure<UserState>::InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback)
    : m_bvh(std::move(buildBVH(sceneObjects)))
    , m_hitCallback(hitCallback)
    , m_missCallback(missCallback)
{
}

template <typename UserState>
inline void InCoreBatchingAccelerationStructure<UserState>::placeIntersectRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        SurfaceInteraction si;
        Ray ray = rays[i]; // Copy so we can mutate it

        if (m_bvh.intersect(ray, si)) {
            assert(si.sceneObject);
            m_hitCallback(ray, si, perRayUserData[i], nullptr);
        } else {
            m_missCallback(ray, perRayUserData[i]);
        }
    }
}

template <typename UserState>
inline PauseableBVH4<typename InCoreBatchingAccelerationStructure<UserState>::TopLevelLeafNode> InCoreBatchingAccelerationStructure<UserState>::buildBVH(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects)
{
    std::vector<TopLevelLeafNode> leafs;
    std::vector<Bounds> bounds;

    // Group small SceneObjects together into a single leaf node to create a better balance in the bottom level trees
    unsigned numPrimitivesInCurrentLeaf = 0;
    std::vector<const SceneObject*> sceneObjectsCurrentLeaf;
    Bounds currentLeafBounds;
    for (const auto& sceneObject : sceneObjects) {
        const unsigned numPrimitives = sceneObject->getMeshRef().numTriangles();
        ALWAYS_ASSERT(numPrimitives < PRIMITIVES_PER_LEAF);
        if (numPrimitivesInCurrentLeaf + numPrimitives >= PRIMITIVES_PER_LEAF) {
            leafs.emplace_back(sceneObjectsCurrentLeaf);
            bounds.emplace_back(currentLeafBounds);

            numPrimitivesInCurrentLeaf = 0;
            sceneObjectsCurrentLeaf.clear();
            currentLeafBounds = Bounds();
        }

        numPrimitivesInCurrentLeaf += numPrimitives;
        sceneObjectsCurrentLeaf.push_back(sceneObject.get());
        currentLeafBounds.extend(sceneObject->getMeshRef().getBounds());
    }
    if (numPrimitivesInCurrentLeaf > 0) {
        leafs.emplace_back(sceneObjectsCurrentLeaf);
        bounds.emplace_back(currentLeafBounds);
    }

    auto result = PauseableBVH4<TopLevelLeafNode>(leafs, bounds);
    return std::move(result);
}

template <typename UserState>
inline unsigned InCoreBatchingAccelerationStructure<UserState>::BotLevelLeafNode::numPrimitives() const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().numTriangles();
}

template <typename UserState>
inline Bounds InCoreBatchingAccelerationStructure<UserState>::BotLevelLeafNode::getPrimitiveBounds(unsigned primitiveID) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().getPrimitiveBounds(primitiveID);
}

template <typename UserState>
inline bool InCoreBatchingAccelerationStructure<UserState>::BotLevelLeafNode::intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);

    float tHit;
    bool hit = sceneObject->getMeshRef().intersectPrimitive(primitiveID, ray, tHit, si);
    if (hit) {
        si.sceneObject = sceneObject;
        si.primitiveID = primitiveID;
        ray.tfar = tHit;
        assert(si.sceneObject);
    }
    return hit;
}

template <typename UserState>
inline InCoreBatchingAccelerationStructure<UserState>::TopLevelLeafNode::TopLevelLeafNode(gsl::span<const SceneObject*> sceneObjects)
    : m_leafBVH(std::move(buildBVH(sceneObjects)))
{
}

template <typename UserState>
inline bool InCoreBatchingAccelerationStructure<UserState>::TopLevelLeafNode::intersect(Ray& ray, SurfaceInteraction& si, PauseableBVHInsertHandle insertHandle) const
{
    (void)insertHandle;
    return m_leafBVH.intersect(ray, si);
}

template <typename UserState>
inline WiVeBVH8Build8<typename InCoreBatchingAccelerationStructure<UserState>::BotLevelLeafNode> InCoreBatchingAccelerationStructure<UserState>::TopLevelLeafNode::buildBVH(gsl::span<const SceneObject*> sceneObjects)
{
    std::vector<const BotLevelLeafNode*> leafs(sceneObjects.size());
    std::transform(std::begin(sceneObjects), std::end(sceneObjects), std::begin(leafs), [](const SceneObject* sceneObject) {
        return reinterpret_cast<const BotLevelLeafNode*>(sceneObject);
    });

    WiVeBVH8Build8<BotLevelLeafNode> bvh;
    bvh.build(leafs);
    return std::move(bvh);
}

}
