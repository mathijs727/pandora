#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/memory_arena_ts.h"
#include <array>
#include <atomic>
#include <bitset>
#include <gsl/gsl>
#include <memory>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 1024;

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
        TopLevelLeafNode(const SceneObject* sceneObjects);
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
    template <size_t Size>
    struct RayBatch {
    public:
        bool tryPush(const Ray& ray, const UserState& state, SurfaceInteraction& si);

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray&, UserState&, SurfaceInteraction&>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            explicit iterator(const RayBatch* batch);
            iterator& operator++(); // pre-increment
            iterator operator++(int); // post-increment
            bool operator==(iterator other) const;
            bool operator!=(iterator other) const;
            value_type operator*() const;

        private:
            const RayBatch* m_rayBatch;
            size_t m_index;
        };

        const iterator begin() const;
        const iterator end() const;

    private:
        std::array<Ray, Size> m_rays;
        std::array<UserState, Size> m_userStates;
        std::array<SurfaceInteraction, Size> m_surfaceInteractions; // TODO: only keep track of this for rays that have already hit something
        std::bitset<Size> m_isValid; // Mask indicating whether the an item is filled with valid data
    };

    PauseableBVH4<TopLevelLeafNode> m_bvh;

    HitCallback m_hitCallback;
    MissCallback m_missCallback;
};

template <typename UserState>
template <size_t Size>
inline const typename InCoreBatchingAccelerationStructure<UserState>::template RayBatch<Size>::iterator InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::begin() const
{
    return iterator(this);
}

template <typename UserState>
template <size_t Size>
inline const typename InCoreBatchingAccelerationStructure<UserState>::template RayBatch<Size>::iterator InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::end() const
{
    return iterator(this);
}

template <typename UserState>
template <size_t Size>
inline InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::iterator(const InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>* batch)
    : m_rayBatch(batch)
    , m_index(0)
{
}

template <typename UserState>
template <size_t Size>
inline typename InCoreBatchingAccelerationStructure<UserState>::template RayBatch<Size>::iterator& InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::operator++()
{
    m_index++;
    while (!m_rayBatch->m_isValid[m_index] && m_index < Size)
        m_index++;
    return *this;
}

template <typename UserState>
template <size_t Size>
inline typename InCoreBatchingAccelerationStructure<UserState>::template RayBatch<Size>::iterator InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::operator++(int)
{
    auto r = *this;
    m_index++;
    while (!m_rayBatch->m_isValid[m_index] && m_index < Size)
        m_index++;
    return r;
}

template <typename UserState>
template <size_t Size>
inline bool InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::operator==(InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index == other.m_index;
}

template <typename UserState>
template <size_t Size>
inline bool InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::operator!=(InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index != other.m_index;
}

template <typename UserState>
template <size_t Size>
inline std::tuple<Ray&, UserState&, SurfaceInteraction&> InCoreBatchingAccelerationStructure<UserState>::RayBatch<Size>::iterator::operator*() const
{
    return { m_rayBatch->m_rays[m_index], m_rayBatch->m_userStates[m_index], m_rayBatch->m_surfaceInteractions[m_index] };
}

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
    for (const auto& sceneObject : sceneObjects) {
        leafs.emplace_back(sceneObject.get());
        bounds.emplace_back(sceneObject->getMeshRef().getBounds());
    }

    return PauseableBVH4<TopLevelLeafNode>(leafs, bounds);
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
inline InCoreBatchingAccelerationStructure<UserState>::TopLevelLeafNode::TopLevelLeafNode(const SceneObject* sceneObject)
    : m_leafBVH(std::move(buildBVH(gsl::make_span(&sceneObject, 1))))
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
