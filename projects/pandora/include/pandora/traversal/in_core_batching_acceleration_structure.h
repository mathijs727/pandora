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
#include <optional>
#include <tbb/concurrent_unordered_set.h>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_sort.h>
#include <tbb/reader_writer_lock.h>
#include <tbb/task_group.h>

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 1024;
static constexpr bool ENABLE_BATCHING = true;

template <typename UserState, size_t BatchSize = 64>
class InCoreBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
    ~InCoreBatchingAccelerationStructure() = default;

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);
    void placeIntersectAnyRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

    void flush();

private:
    static constexpr unsigned PRIMITIVES_PER_LEAF = IN_CORE_BATCHING_PRIMS_PER_LEAF;

    struct RayBatch {
    public:
        RayBatch(RayBatch* nextPtr = nullptr)
            : m_nextPtr(nextPtr)
        {
        }
        void setNext(RayBatch* nextPtr) { m_nextPtr = nextPtr; }
        RayBatch* next() { return m_nextPtr; }
        bool full() const { return m_data.full(); }

        bool tryPush(const Ray& ray, const RayHit& hitInfo, const UserState& state, const PauseableBVHInsertHandle& insertHandle);
        bool tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle);

        size_t size() const
        {
            return m_data.size();
        }

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray&, std::optional<RayHit>&, UserState&, PauseableBVHInsertHandle&>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            explicit iterator(RayBatch* batch, size_t index);
            iterator& operator++(); // pre-increment
            iterator operator++(int); // post-increment
            bool operator==(iterator other) const;
            bool operator!=(iterator other) const;
            value_type operator*();

        private:
            RayBatch* m_rayBatch;
            size_t m_index;
        };

        const iterator begin();
        const iterator end();

    private:
        // NOTE: using a std::array here is expensive because the default constructor of all items will be called when the batch is created
        // The larger the batches, the more expensive this becomes
        struct BatchItem {
            BatchItem(const Ray& ray, const std::optional<RayHit>& rayHit, const UserState& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , rayHit(rayHit)
                , userState(userState)
                , insertHandle(insertHandle)
            {
            }
            Ray ray;
            std::optional<RayHit> rayHit;
            UserState userState;
            PauseableBVHInsertHandle insertHandle;
        };
        eastl::fixed_vector<BatchItem, BatchSize> m_data;

        RayBatch* m_nextPtr;
    };

    class BotLevelLeafNode {
    public:
        BotLevelLeafNode(const GeometricSceneObject* sceneObject, unsigned primitiveID)
            : m_sceneObject(sceneObject)
            , m_primitiveID(primitiveID)
        {
        }
        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        const GeometricSceneObject* m_sceneObject;
        const unsigned m_primitiveID;
    };

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode(TopLevelLeafNode&& other);
        TopLevelLeafNode(
            const SceneObject* sceneObject,
            const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>& bvh,
            InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        void prepareForFlushUnsafe(); // Adds the active batches to the list of immutable batches (even if they are not full)
        size_t flush(); // Flushes the list of immutable batches (but not the active batches)

    private:
        //static WiVeBVH8Build8<BotLevelLeafNode> buildBVH(gsl::span<const SceneObject*> sceneObject);

    private:
        const SceneObject* m_sceneObject;
        std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>> m_leafBVH;

        tbb::enumerable_thread_specific<RayBatch*> m_threadLocalActiveBatch;
        std::atomic<RayBatch*> m_immutableRayBatchList;
        InCoreBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(gsl::span<const std::unique_ptr<SceneObject>>, InCoreBatchingAccelerationStructure& accelerationStructure);

private:
    GrowingFreeListTS<RayBatch> m_batchAllocator;
    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;
    tbb::enumerable_thread_specific<RayBatch*> m_threadLocalPreallocatedRaybatch;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::InCoreBatchingAccelerationStructure(gsl::span<const std::unique_ptr<SceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_batchAllocator()
    , m_bvh(std::move(buildBVH(sceneObjects, *this)))
    , m_threadLocalPreallocatedRaybatch([&]() { return m_batchAllocator.allocate(); })
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
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

        auto optResult = m_bvh.intersect(ray, hitInfo, userState);
        if (optResult) {
            if (*optResult) {
                // We got the result immediately (traversal was not paused)
                SurfaceInteraction si = hitInfo.sceneObject->fillSurfaceInteraction(ray, hitInfo);
                m_hitCallback(ray, si, userState, nullptr);
            } else {
                m_missCallback(ray, userState);
            }
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::placeIntersectAnyRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        auto optResult = m_bvh.intersectAny(ray, userState);
        if (optResult) {
            if (*optResult) {
                // We got the result immediately (traversal was not paused)
                m_anyHitCallback(ray, userState);
            } else {
                m_missCallback(ray, userState);
            }
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::flush()
{
    while (true) {
#ifndef NDEBUG
        for (auto* topLevelLeafNode : m_bvh.leafs())
            topLevelLeafNode->prepareForFlushUnsafe();

        //tbb::concurrent_vector<TopLevelLeafNode*> leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);

        size_t raysProcessed = 0;
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

        /*tbb::concurrent_vector<TopLevelLeafNode*> leafsWithBatchedRays = std::move(m_leafsWithBatchedRays);
        if (leafsWithBatchedRays.empty())
            break;*/

        // Sort nodes so the ones with the most batches get processed first
        //tbb::parallel_sort(leafsWithBatchedRays, [](const TopLevelLeafNode* a, const TopLevelLeafNode* b) -> bool { return a->approximateBatchCount() < b->approximateBatchCount(); });

        tbb::enumerable_thread_specific<size_t> raysProcessedTL([]() -> size_t { return 0; });
        tbb::parallel_for_each(m_bvh.leafs(), [&](auto* topLevelLeafNode) {
            raysProcessedTL.local() += topLevelLeafNode->flush();
        });
        size_t raysProcessed = std::accumulate(std::begin(raysProcessedTL), std::end(raysProcessedTL), (size_t)0, std::plus<size_t>());
        if (raysProcessed == 0)
            break;
#endif
    }

    std::cout << "FLUSH COMPLETE" << std::endl;
}

template <typename UserState, size_t BatchSize>
inline PauseableBVH4<typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> InCoreBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    gsl::span<const std::unique_ptr<SceneObject>> sceneObjects,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
{
    // Find all referenced geometric scene objects
    std::unordered_set<const GeometricSceneObject*> uniqueGeometricSceneObjects;
    for (const auto& sceneObject : sceneObjects) {
        if (auto instancedSceneObject = dynamic_cast<const InstancedSceneObject*>(sceneObject.get())) {
            uniqueGeometricSceneObjects.insert(instancedSceneObject->getBaseObject());
        } else if (auto geometricSceneObject = dynamic_cast<const GeometricSceneObject*>(sceneObject.get())) {
            uniqueGeometricSceneObjects.insert(geometricSceneObject);
        } else {
            THROW_ERROR("Unknown scene object type!");
        }
    }

    // Build a BVH for each unique GeometricSceneObject
    std::unordered_map<const GeometricSceneObject*, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>> botLevelBVHs;
    for (auto sceneObjectPtr : uniqueGeometricSceneObjects) {
        std::vector<BotLevelLeafNode> leafs;
        for (unsigned primitiveID = 0; primitiveID < sceneObjectPtr->numPrimitives(); primitiveID++) {
            leafs.push_back(BotLevelLeafNode(sceneObjectPtr, primitiveID));
        }

        /*if (leafs.size() <= 4)
        {
            std::cout << "WARNING: skipping SceneObject with too little (" << leafs.size() << ") primitives" << std::endl;
            botLevelBVHs[sceneObjectPtr] = nullptr;
            continue;
        }*/

        auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNode>>();
        bvh->build(leafs);
        botLevelBVHs[sceneObjectPtr] = bvh;
    }

    // Build the final BVH over all (instanced) scene objects
    std::vector<TopLevelLeafNode> leafs;
    for (const auto& sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObject*>(sceneObject.get())) {
            auto bvh = botLevelBVHs[instancedSceneObject->getBaseObject()];
            if (bvh)
                leafs.emplace_back(sceneObject.get(), bvh, accelerationStructure);
        } else if (const auto* geometricSceneObject = dynamic_cast<const GeometricSceneObject*>(sceneObject.get())) {
            auto bvh = botLevelBVHs[geometricSceneObject];
            if (bvh)
                leafs.emplace_back(sceneObject.get(), bvh, accelerationStructure);
        }
    }
    return PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    const SceneObject* sceneObject,
    const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>& bvh,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
    : m_sceneObject(sceneObject)
    , m_leafBVH(bvh)
    , m_threadLocalActiveBatch([]() { return nullptr; })
    , m_immutableRayBatchList(nullptr)
    , m_accelerationStructurePtr(&accelerationStructure)
{
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_sceneObject(other.m_sceneObject)
    , m_leafBVH(std::move(other.m_leafBVH))
    , m_threadLocalActiveBatch(std::move(other.m_threadLocalActiveBatch))
    , m_immutableRayBatchList(other.m_immutableRayBatchList.load())
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
{
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::getBounds() const
{
    return m_sceneObject->worldBounds();
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    if constexpr (ENABLE_BATCHING) {
        RayBatch* batch = mutThisPtr->m_threadLocalActiveBatch.local();
        if (!batch || batch->full()) {
            if (batch) {
                // Batch was full, move it to the list of immutable batches
                auto* oldHead = mutThisPtr->m_immutableRayBatchList.load();
                do {
                    batch->setNext(oldHead);
                } while (!mutThisPtr->m_immutableRayBatchList.compare_exchange_weak(oldHead, batch));
            }

            // Allocate a new batch and set it as the new active batch
            batch = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate();
            mutThisPtr->m_threadLocalActiveBatch.local() = batch;
        }

        bool success = batch->tryPush(ray, rayInfo, userState, insertHandle);
        assert(success);

        return {}; // Paused*/
    } else {
        if (m_sceneObject->isInstancedSceneObject()) {
            const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObject*>(m_sceneObject);
            auto localRay = instancedSceneObject->transformRayToInstanceSpace(ray);
            if (mutThisPtr->m_leafBVH->intersect(localRay, rayInfo)) {
                ray.tfar = localRay.tfar;
                rayInfo.sceneObject = m_sceneObject;
                return true;
            } else {
                return false;
            }
        } else {
            if (mutThisPtr->m_leafBVH->intersect(ray, rayInfo)) {
                rayInfo.sceneObject = m_sceneObject;
                return true;
            } else {
                return false;
            }
        }
    }
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    if constexpr (ENABLE_BATCHING) {
        RayBatch* batch = mutThisPtr->m_threadLocalActiveBatch.local();
        if (!batch || batch->full()) {
            if (batch) {
                // Batch was full, move it to the list of immutable batches
                auto* oldHead = mutThisPtr->m_immutableRayBatchList.load();
                do {
                    batch->setNext(oldHead);
                } while (!mutThisPtr->m_immutableRayBatchList.compare_exchange_weak(oldHead, batch));
            }

            // Allocate a new batch and set it as the new active batch
            batch = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate();
            mutThisPtr->m_threadLocalActiveBatch.local() = batch;
        }

        bool success = batch->tryPush(ray, userState, insertHandle);
        assert(success);

        return {}; // Paused
    } else {
        if (m_sceneObject->isInstancedSceneObject()) {
            const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObject*>(m_sceneObject);
            auto localRay = instancedSceneObject->transformRayToInstanceSpace(ray);
            return m_leafBVH->intersectAny(localRay);
        } else {
            return m_leafBVH->intersectAny(ray);
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::prepareForFlushUnsafe()
{
    for (auto& batch : m_threadLocalActiveBatch) {
        if (batch) {
            batch->setNext(m_immutableRayBatchList);
            m_immutableRayBatchList = batch;
        }
        batch = nullptr;
    }
}

template <typename UserState, size_t BatchSize>
inline size_t InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flush()
{
    RayBatch* batch = m_immutableRayBatchList.exchange(nullptr);
    if (!batch)
        return false;

    size_t numRays = 0;
    tbb::task_group taskGroup;
    while (batch) {
        numRays += batch->size();

        auto next = batch->next();
        taskGroup.run([=]() {
            if (m_sceneObject->isInstancedSceneObject()) {
                const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObject*>(m_sceneObject);
                for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
                    auto localRay = instancedSceneObject->transformRayToInstanceSpace(ray);

                    // Intersect with the bottom-level BVH
                    if (hitInfo) {
                        if (m_leafBVH->intersect(localRay, *hitInfo)) {
                            hitInfo->sceneObject = m_sceneObject; // Set to the actual (specific instance) scene object
                            ray.tfar = localRay.tfar;
                        }
                    } else {
                        if (m_leafBVH->intersectAny(localRay))
                            ray.tfar = localRay.tfar;
                    }
                }
            } else {
                for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
                    // Intersect with the bottom-level BVH
                    if (hitInfo) {
                        if (m_leafBVH->intersect(ray, *hitInfo))
                            hitInfo->sceneObject = m_sceneObject;
                    } else {
                        m_leafBVH->intersectAny(ray);
                    }
                }
            }

            for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
                if (hitInfo) {
                    // Insert the ray back into the top-level  BVH
                    auto optResult = m_accelerationStructurePtr->m_bvh.intersect(ray, *hitInfo, userState, insertHandle);
                    if (optResult) {
                        // Ray exited the system so hitInfo contains the closest hit
                        if (hitInfo->sceneObject) {
                            // Compute the full surface interaction
                            SurfaceInteraction si = hitInfo->sceneObject->fillSurfaceInteraction(ray, *hitInfo);
                            m_accelerationStructurePtr->m_hitCallback(ray, si, userState, nullptr);
                        } else {
                            m_accelerationStructurePtr->m_missCallback(ray, userState);
                        }
                    }
                } else {
                    // Intersect any
                    if (ray.tfar == -std::numeric_limits<float>::infinity()) {
                        m_accelerationStructurePtr->m_anyHitCallback(ray, userState);
                    } else {
                        auto optResult = m_accelerationStructurePtr->m_bvh.intersectAny(ray, userState, insertHandle);
                        if (optResult && *optResult == false) {
                            // Ray exited system
                            m_accelerationStructurePtr->m_missCallback(ray, userState);
                        }
                    }
                }
            }
            m_accelerationStructurePtr->m_batchAllocator.deallocate(batch);
        });
        batch = next;
    }
    taskGroup.wait();

    return numRays;
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::getBounds() const
{
    return m_sceneObject->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    return m_sceneObject->intersectPrimitive(ray, hitInfo, m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const RayHit& hitInfo, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, hitInfo, state, insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    std::optional<RayHit> opt = {};
    m_data.emplace_back(ray, opt, state, insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::begin()
{
    return iterator(this, 0);
}

template <typename UserState, size_t BatchSize>
inline const typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::end()
{
    return iterator(this, m_data.size());
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::iterator(InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch* batch, size_t index)
    : m_rayBatch(batch)
    , m_index(index)
{
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator& InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++()
{
    m_index++;
    return *this;
}

template <typename UserState, size_t BatchSize>
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++(int)
{
    auto r = *this;
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
inline typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::value_type InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator*()
{
    auto& [ray, hitInfo, userState, insertHandle] = m_rayBatch->m_data[m_index];
    return { ray, hitInfo, userState, insertHandle };
}
}
