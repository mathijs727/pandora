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
#include <tbb/concurrent_unordered_set.h>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_sort.h>
#include <tbb/reader_writer_lock.h>
#include <tbb/task_group.h>

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 2048;

template <typename UserState, size_t BatchSize = 50000>
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
        {
        }
        void setNext(RayBatch* nextPtr) { m_nextPtr = nextPtr; }
        RayBatch* next() { return m_nextPtr; }
        bool full() const { return m_sharedIndex.load() == BatchSize; }

        bool tryPush(const Ray& ray, const RayHit& hitInfo, const UserState& state, const PauseableBVHInsertHandle& insertHandle);

        size_t size() const
        {
            return std::min(BatchSize, m_sharedIndex.load());
        }
        std::tuple<Ray&, RayHit&, UserState&, PauseableBVHInsertHandle&> getUnsafe(int i);

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray, RayHit, UserState, PauseableBVHInsertHandle>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            explicit iterator(RayBatch* batch, int index);
            iterator& operator++(); // pre-increment
            iterator operator++(int); // post-increment
            bool operator==(iterator other) const;
            bool operator!=(iterator other) const;
            value_type operator*();

        private:
            RayBatch* m_rayBatch;
            int m_index;
        };

        const iterator begin();
        const iterator end();

    private:
        std::array<Ray, BatchSize> m_rays;
        std::array<RayHit, BatchSize> m_hitInfos;
        std::array<UserState, BatchSize> m_userStates;
        std::array<PauseableBVHInsertHandle, BatchSize> m_insertHandles;

        RayBatch* m_nextPtr;
        //size_t m_index;
        std::atomic_size_t m_sharedIndex;
    };

    class BotLevelLeafNode {
    public:
        unsigned numPrimitives() const;
        Bounds getPrimitiveBounds(unsigned primitiveID) const;
        bool intersectPrimitive(Ray& ray, RayHit& hitInfo, unsigned primitiveID) const;
    };

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode(TopLevelLeafNode&& other);
        TopLevelLeafNode(const SceneObject* sceneObjects, InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure);

        bool intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        void prepareForFlushUnsafe(); // Adds the current batch to the list of immutable batches (even if it is not full)
        int flush(); // Flushes the list of immutable batches (but not the active batches)

    private:
        static WiVeBVH8Build8<BotLevelLeafNode> buildBVH(gsl::span<const SceneObject*> sceneObject);

    private:
        WiVeBVH8Build8<BotLevelLeafNode> m_leafBVH;
        //tbb::enumerable_thread_specific<RayBatch*> m_threadLocalActiveBatch;
        //std::atomic<RayBatch*> m_immutableRayBatchList;
        std::atomic<RayBatch*> m_currentRayBatch;
        std::atomic_int m_numBatchesRays;
        InCoreBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(gsl::span<const std::unique_ptr<SceneObject>>, InCoreBatchingAccelerationStructure& accelerationStructure);

private:
    tbb::concurrent_vector<TopLevelLeafNode*> m_leafsWithBatchedRays;
    GrowingFreeListTS<RayBatch> m_batchAllocator;
    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;
    tbb::enumerable_thread_specific<RayBatch*> m_threadLocalPreallocatedRaybatch;

    std::atomic_size_t m_raysInSystem;

    HitCallback m_hitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, MissCallback missCallback)
    : m_batchAllocator()
    , m_bvh(std::move(buildBVH(sceneObjects, *this)))
    , m_threadLocalPreallocatedRaybatch([&]() { return m_batchAllocator.allocate(); })
    , m_raysInSystem(0)
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
        RayHit hitInfo;
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        if (m_bvh.intersect(ray, hitInfo, userState)) {
            // We got the result immediately (traversal was not paused)
            if (hitInfo.sceneObject) {
                ALWAYS_ASSERT(false);
                // Compute the full surface interaction
                SurfaceInteraction si;
                hitInfo.sceneObject->getMeshRef().intersectPrimitive(ray, si, hitInfo.primitiveID);
                si.sceneObject = hitInfo.sceneObject;
                si.primitiveID = hitInfo.primitiveID;

                m_hitCallback(ray, si, userState, nullptr);
            } else {
                m_missCallback(ray, userState);
            }
        } else {
            m_raysInSystem.fetch_add(1);
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::flush()
{
    std::cout << "RAYS IN BEFORE FLUSH SYSTEM: " << m_raysInSystem << std::endl;

    //while (!leafsWithBatchedRays.empty()) {
    while (true) {
//#ifndef NDEBUG
#if 1
        for (auto* topLevelLeafNode : m_bvh.leafs())
            topLevelLeafNode->prepareForFlushUnsafe();

        tbb::concurrent_vector<TopLevelLeafNode*> leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);

        int raysProcessed = 0;
        for (auto* topLevelLeafNode : m_bvh.leafs()) {
            raysProcessed += topLevelLeafNode->flush();
        }

        if (raysProcessed == 0) {
            std::cout << "No rays left" << std::endl;
            break;
        } else {
            std::cout << raysProcessed << " rays processed" << std::endl;
        }
#else
        tbb::parallel_for_each(m_bvh.leafs(), [](auto* topLevelLeafNode) {
            topLevelLeafNode->prepareForFlushUnsafe();
        });

        //tbb::concurrent_vector<TopLevelLeafNode*> leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);
        //if (leafsWithBatchedRays.empty())
        //    break;

        // Sort nodes so the ones with the most batches get processed first
        //tbb::parallel_sort(leafsWithBatchedRays, [](const TopLevelLeafNode* a, const TopLevelLeafNode* b) -> bool { return a->approximateBatchCount() < b->approximateBatchCount(); });

        tbb::enumerable_thread_specific<bool> anyRayLeftTL([]() { return false; });
        tbb::parallel_for_each(m_bvh.leafs(), [&](auto* topLevelLeafNode) {
            anyRayLeftTL.local() = anyRayLeftTL.local() || topLevelLeafNode->flush();
        });
        bool anyRayLeft = std::reduce(std::begin(anyRayLeftTL), std::end(anyRayLeftTL), false, [](bool a, bool b) { return a || b; });
        if (!anyRayLeft)
            break;
#endif
    }

    ALWAYS_ASSERT(m_raysInSystem.load() == 0);

    std::cout << "FLUSH COMPLETE" << std::endl;
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
    //, m_threadLocalActiveBatch([]() { return nullptr; })
    , m_currentRayBatch(nullptr)
    , m_numBatchesRays(0)
    , m_accelerationStructurePtr(&accelerationStructure)
{
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_leafBVH(std::move(other.m_leafBVH))
    //, m_threadLocalActiveBatch(std::move(other.m_threadLocalActiveBatch))
    , m_currentRayBatch(other.m_currentRayBatch.load())
    , m_numBatchesRays(other.m_numBatchesRays.load())
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
{
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    RayBatch* batch = mutThisPtr->m_currentRayBatch.load();
    while (true) {
        if (batch && batch->tryPush(ray, rayInfo, userState, insertHandle)) {
            mutThisPtr->m_numBatchesRays.fetch_add(1);
            break;
        }

        auto& preallocatedBatch = mutThisPtr->m_accelerationStructurePtr->m_threadLocalPreallocatedRaybatch.local();
        preallocatedBatch->setNext(batch);
        if (mutThisPtr->m_currentRayBatch.compare_exchange_strong(batch, preallocatedBatch)) {
            batch = preallocatedBatch;// Value only gets set automatically on failure
            mutThisPtr->m_accelerationStructurePtr->m_threadLocalPreallocatedRaybatch.local() = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate();
        }
    }

    /*RayBatch* batch = mutThisPtr->m_threadLocalActiveBatch.local();
    if (!batch || batch->full()) {
        if (batch) {

            // Batch was full, move it to the list of immutable batches
            auto* oldHead = mutThisPtr->m_immutableRayBatchList.load();
            do {
                batch->setNext(oldHead);
            } while (!mutThisPtr->m_immutableRayBatchList.compare_exchange_weak(oldHead, batch));

            if (!oldHead)
                mutThisPtr->m_accelerationStructurePtr->m_leafsWithBatchedRays.push_back(mutThisPtr);
        }

        // Allocate a new batch and set it as the new active batch
        batch = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate();
        mutThisPtr->m_threadLocalActiveBatch.local() = batch;
    }

    bool success = batch->tryPush(ray, rayInfo, userState, insertHandle);
    ALWAYS_ASSERT(success);
    mutThisPtr->m_numBatchesRays.fetch_add(1);*/

    return false;
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::prepareForFlushUnsafe()
{
    /*for (auto& batch : m_threadLocalActiveBatch) {
        if (batch) {
            if (!m_immutableRayBatchList)
                m_accelerationStructurePtr->m_leafsWithBatchedRays.push_back(this);

            batch->setNext(m_immutableRayBatchList);
            m_immutableRayBatchList = batch;
        }
        batch = nullptr;
    }*/
}

template <typename UserState, size_t BatchSize>
inline int InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flush()
{
    //std::lock_guard<std::mutex> lock(m_rayBatchListMutex);
    RayBatch* batch = m_currentRayBatch.exchange(nullptr);
    if (!batch)
        return false;

    int numRaysBeforeFlush = m_numBatchesRays.exchange(0);

    int numRays = 0;
    //tbb::task_group taskGroup;
    while (batch) {
        //taskGroup.run([=]() {
        //for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
        for (int i = (int)batch->size() - 1; i >= 0; i--) {
            auto [ray, hitInfo, userState, insertHandle] = batch->getUnsafe(i);

            // Intersect with the bottom-level BVH
            static_assert(std::is_same_v<RayHit, std::decay_t<decltype(hitInfo)>>);
            m_leafBVH.intersect(ray, hitInfo);
            numRays++;
        //}

        //for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
        //for (int i = 0; i < batch->size(); i++) {
        //for (int i = (int)batch->size() - 1; i >= 0; i--) {
            //auto [ray, hitInfo, userState, insertHandle] = batch->getUnsafe(i);

            // Insert the ray back into the top-level  BVH
            if (m_accelerationStructurePtr->m_bvh.intersect(ray, hitInfo, userState, insertHandle)) {
                // Ray exited the system => call callbacks
                if (hitInfo.sceneObject) {
                    // Compute the full surface interaction
                    SurfaceInteraction si;
                    hitInfo.sceneObject->getMeshRef().intersectPrimitive(ray, si, hitInfo.primitiveID);
                    si.sceneObject = hitInfo.sceneObject;
                    si.primitiveID = hitInfo.primitiveID;
                    m_accelerationStructurePtr->m_hitCallback(ray, si, userState, nullptr);
                } else {
                    m_accelerationStructurePtr->m_missCallback(ray, userState);
                }

                m_accelerationStructurePtr->m_raysInSystem.fetch_sub(1);
            }
        }
        auto next = batch->next();
        m_accelerationStructurePtr->m_batchAllocator.deallocate(batch);
        batch = next;
        //});
    }
    //taskGroup.wait();

    //ALWAYS_ASSERT(numRaysBeforeFlush == m_numBatchesRays.load());
    ALWAYS_ASSERT(numRays == numRaysBeforeFlush);

    return numRays;
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
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersectPrimitive(Ray& ray, RayHit& hitInfo, unsigned primitiveID) const
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

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const RayHit& hitInfo, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    auto index = m_sharedIndex.fetch_add(1);
    if (index >= BatchSize)
        return false;

    m_rays[index] = ray;
    m_userStates[index] = state;
    m_hitInfos[index] = hitInfo;
    m_insertHandles[index] = insertHandle;

    return true;
}

template <typename UserState, size_t BatchSize>
inline std::tuple<Ray&, RayHit&, UserState&, PauseableBVHInsertHandle&> InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::getUnsafe(int i)
{
    return { m_rays[i], m_hitInfos[i], m_userStates[i], m_insertHandles[i] };
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::begin()
{
    //return iterator(this, 0);
    return iterator(this, (int)std::min(BatchSize - 1, m_sharedIndex.load() - 1));
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::end()
{
    //return iterator(this, (int)std::min(BatchSize, m_sharedIndex.load()));
    return iterator(this, -1);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::iterator(InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch* batch, int index)
    : m_rayBatch(batch)
    , m_index(index)
{
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator& InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++()
{
    m_index--;
    return *this;
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++(int)
{
    auto r = *this;
    m_index--;
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
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::value_type InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator*()
{
    return { m_rayBatch->m_rays[m_index], m_rayBatch->m_hitInfos[m_index], m_rayBatch->m_userStates[m_index], m_rayBatch->m_insertHandles[m_index] };
}
}
