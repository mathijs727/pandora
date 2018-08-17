#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/growing_free_list_ts.h"
#include <array>
#include <atomic>
#include <bitset>
#include <gsl/gsl>
#include <memory>
#include <numeric>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <tbb/reader_writer_lock.h>
#include <tbb/parallel_sort.h>

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 8096;

template <typename UserState, size_t BatchSize = 1024>
class InCoreBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback);
    ~InCoreBatchingAccelerationStructure() = default;

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

    void flush();

private:
    static constexpr unsigned PRIMITIVES_PER_LEAF = IN_CORE_BATCHING_PRIMS_PER_LEAF;

    struct RayBatch {
    public:
        RayBatch(RayBatch* nextPtr = nullptr)
            : m_nextPtr(nextPtr)
            , m_sharedIndex(0)
            , m_threadLocalIndex()
        {
            std::fill(std::begin(m_isValid), std::end(m_isValid), false);
        }
        RayBatch* next() { return m_nextPtr; }

        bool tryPush(const Ray& ray, const SurfaceInteraction& si, const UserState& state, const PauseableBVHInsertHandle& insertHandle);

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray, SurfaceInteraction, UserState, PauseableBVHInsertHandle>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            explicit iterator(const RayBatch* batch, size_t index);
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
        std::array<Ray, BatchSize> m_rays;
        std::array<SurfaceInteraction, BatchSize> m_surfaceInteractions; // TODO: only keep track of this for rays that have already hit something
        std::array<UserState, BatchSize> m_userStates;
        std::array<PauseableBVHInsertHandle, BatchSize> m_insertHandles;
        std::array<bool, BatchSize> m_isValid; // Indicates whether each item is filled with valid data (using bitset here is not thread safe)

        RayBatch* m_nextPtr;
        std::atomic_size_t m_sharedIndex;
        struct ThreadLocalBlock {
            size_t index = 0;
            size_t space = 0;
        };
        tbb::enumerable_thread_specific<ThreadLocalBlock> m_threadLocalIndex;
    };

    class BotLevelLeafNode {
    public:
        unsigned numPrimitives() const;
        Bounds getPrimitiveBounds(unsigned primitiveID) const;
        inline bool intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const;
    };

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode() = default;
        TopLevelLeafNode(TopLevelLeafNode&& other);
        TopLevelLeafNode(const SceneObject* sceneObjects, InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure);
        TopLevelLeafNode& operator=(TopLevelLeafNode&& other);

        bool intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        uint32_t numFullBatches();
        void flush(); // Traverses all rays through the lower-level BVH. This function is thread-safe.

    private:
        static WiVeBVH8Build8<BotLevelLeafNode> buildBVH(gsl::span<const SceneObject*> sceneObject);

    private:
        WiVeBVH8Build8<BotLevelLeafNode> m_leafBVH;
        tbb::reader_writer_lock m_changeBatchLock;
        RayBatch* m_currentBatch;
        uint32_t m_numFullBatches;
        InCoreBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(gsl::span<const std::unique_ptr<SceneObject>>, InCoreBatchingAccelerationStructure& accelerationStructure);

private:
    tbb::concurrent_vector<TopLevelLeafNode*> m_leafsWithBatchedRays;
    std::vector<TopLevelLeafNode*> m_leafNodes;
    GrowingFreeListTS<RayBatch> m_batchAllocator;
    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;

    HitCallback m_hitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback)
    : m_batchAllocator()
    , m_bvh(std::move(buildBVH(sceneObjects, *this)))
    , m_hitCallback(hitCallback)
    , m_missCallback(missCallback)
{
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::placeIntersectRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        SurfaceInteraction si;
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        if (m_bvh.intersect(ray, si, userState)) {
            // We got the result immediately (traversal was not paused)
            if (si.sceneObject) {
                m_hitCallback(ray, si, userState, nullptr);
            } else {
                m_missCallback(ray, userState);
            }
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::flush()
{
    tbb::concurrent_vector<TopLevelLeafNode*> leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);

    while (!leafsWithBatchedRays.empty()) {
#ifndef NDEBUG
        for (auto* topLevelLeafNode : leafsWithBatchedRays) {
            topLevelLeafNode->flush();
        }
#else
        // Sort full batches first
        tbb::parallel_sort(leafsWithBatchedRays, [](auto* node1, auto* node2) -> bool {
            return node1->numFullBatches() > node2->numFullBatches();
        });
        tbb::parallel_for_each(leafsWithBatchedRays, [](auto* topLevelLeafNode) {
            topLevelLeafNode->flush();
        });
#endif

        leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);
    }
}

template <typename UserState, size_t BatchSize>
inline PauseableBVH4<typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> InCoreBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    gsl::span<const std::unique_ptr<SceneObject>> sceneObjects,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
{
    std::vector<TopLevelLeafNode> leafs;
    std::vector<Bounds> bounds;
    for (const auto& sceneObject : sceneObjects) {
        leafs.emplace_back(sceneObject.get(), accelerationStructure);
        bounds.emplace_back(sceneObject->getMeshRef().getBounds());
    }

    return PauseableBVH4<TopLevelLeafNode, UserState>(leafs, bounds);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    const SceneObject* sceneObject,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
    : m_leafBVH(std::move(buildBVH(gsl::make_span(&sceneObject, 1))))
    , m_currentBatch(nullptr)
    , m_accelerationStructurePtr(&accelerationStructure)
{
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_leafBVH(std::move(other.m_leafBVH))
    , m_currentBatch(other.m_currentBatch)
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
{
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode& InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::operator=(TopLevelLeafNode&& other)
{
    m_leafBVH = std::move(other.m_leafBVH);
    m_currentBatch = other.m_currentBatch;
    m_accelerationStructurePtr = other.m_accelerationStructurePtr;
    return *this;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    while (true) {
        RayBatch* oldBatch;
        { // Read lock scope
            tbb::reader_writer_lock::scoped_lock_read readLock(mutThisPtr->m_changeBatchLock);
            oldBatch = mutThisPtr->m_currentBatch;
            if (oldBatch && oldBatch->tryPush(ray, si, userState, insertHandle))
                break; // Push succesfull
        }

        // The current batch is full, try to replace it by a new one
        { // Write lock scope
            tbb::reader_writer_lock::scoped_lock writeLock(mutThisPtr->m_changeBatchLock);
            if (mutThisPtr->m_currentBatch == oldBatch) // Another thread might have beat us to the race and allocated a new block first
            {
                mutThisPtr->m_currentBatch = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate(oldBatch);
                if (oldBatch == nullptr)
                    mutThisPtr->m_accelerationStructurePtr->m_leafsWithBatchedRays.push_back(mutThisPtr);
                mutThisPtr->m_numFullBatches++;
            }
        }
    }

    return false;
}

template <typename UserState, size_t BatchSize>
inline uint32_t InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::numFullBatches()
{
    // Read lock scope
    tbb::reader_writer_lock::scoped_lock_read readLock(m_changeBatchLock);
    return m_numFullBatches;
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flush()
{
    RayBatch* batch;
    {
        tbb::reader_writer_lock::scoped_lock writeLock(m_changeBatchLock);
        batch = m_currentBatch;
        m_currentBatch = nullptr;
        m_numFullBatches = 0;
    }

    while (batch) {
        for (auto& [ray, si, userState, insertHandle] : *batch) {
            // Intersect with the bottom-level BVH
            m_leafBVH.intersect(ray, si);

            // Insert the ray back into the top-level  BVH
            if (m_accelerationStructurePtr->m_bvh.intersect(ray, si, userState, insertHandle)) {
                // Ray exited the system => call callbacks
                if (si.sceneObject) {
                    m_accelerationStructurePtr->m_hitCallback(ray, si, userState, nullptr);
                } else {
                    m_accelerationStructurePtr->m_missCallback(ray, userState);
                }
            }
        }

        auto* next = batch->next();
        m_accelerationStructurePtr->m_batchAllocator.deallocate(batch);
        batch = next;
    }
}

template <typename UserState, size_t BatchSize>
inline WiVeBVH8Build8<typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::buildBVH(gsl::span<const SceneObject*> sceneObjects)
{
    std::vector<const BotLevelLeafNode*> leafs(sceneObjects.size());
    std::transform(std::begin(sceneObjects), std::end(sceneObjects), std::begin(leafs), [](const SceneObject* sceneObject) {
        return reinterpret_cast<const BotLevelLeafNode*>(sceneObject);
    });

    WiVeBVH8Build8<BotLevelLeafNode> bvh;
    bvh.build(leafs);
    return std::move(bvh);
}

template <typename UserState, size_t BatchSize>
inline unsigned InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::numPrimitives() const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().numTriangles();
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::getPrimitiveBounds(unsigned primitiveID) const
{
    // this pointer is actually a pointer to the sceneObject (see constructor)
    auto sceneObject = reinterpret_cast<const SceneObject*>(this);
    return sceneObject->getMeshRef().getPrimitiveBounds(primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersectPrimitive(unsigned primitiveID, Ray& ray, SurfaceInteraction& si) const
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

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const SurfaceInteraction& si, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    static constexpr size_t threadLocalBlockSize = 4;
    static_assert(BatchSize % threadLocalBlockSize == 0);

    auto localBlock = m_threadLocalIndex.local();
    if (localBlock.space == 0) {
        // Reserve a new block from the batch that only this thread may allocate from
        auto newIndex = m_sharedIndex.fetch_add(threadLocalBlockSize);
        if (newIndex >= BatchSize)
            return false; // The batch is full

        localBlock.index = newIndex;
        localBlock.space = threadLocalBlockSize;
    }

    m_rays[localBlock.index] = ray;
    m_userStates[localBlock.index] = state;
    m_surfaceInteractions[localBlock.index] = si;
    m_insertHandles[localBlock.index] = insertHandle;
    m_isValid[localBlock.index] = true;
    localBlock.index++;
    localBlock.space--;
    return true;
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::begin() const
{
    return iterator(this, 0);
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::end() const
{
    return iterator(this, BatchSize);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::iterator(const InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch* batch, size_t index)
    : m_rayBatch(batch)
    , m_index(index)
{
    // The first item could be invalid
    while (m_index < BatchSize && !m_rayBatch->m_isValid[m_index])
        m_index++;
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator& InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++()
{
    m_index++;
    while (m_index < BatchSize && !m_rayBatch->m_isValid[m_index])
        m_index++;
    return *this;
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++(int)
{
    auto r = *this;
    m_index++;
    while (m_index < BatchSize && !m_rayBatch->m_isValid[m_index])
        m_index++;
    return r;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator==(InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index == other.m_index;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator!=(InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index != other.m_index;
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::value_type InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator*() const
{
    assert(m_rayBatch->m_isValid[m_index]);
    return { m_rayBatch->m_rays[m_index], m_rayBatch->m_surfaceInteractions[m_index], m_rayBatch->m_userStates[m_index], m_rayBatch->m_insertHandles[m_index] };
}
}
