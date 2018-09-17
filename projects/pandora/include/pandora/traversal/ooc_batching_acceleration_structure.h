#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/eviction/evictable.h"
#include "pandora/flatbuffers/ooc_batching_generated.h"
#include "pandora/geometry/triangle.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
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

static constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 1024;

template <typename UserState, size_t BatchSize = 64>
class OOCBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    OOCBatchingAccelerationStructure(gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
    ~OOCBatchingAccelerationStructure() = default;

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

    class BotLevelLeafNodeInstanced {
    public:
        BotLevelLeafNodeInstanced(const SceneObjectGeometry& baseSceneObjectGeometry, unsigned primitiveID);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        const SceneObjectGeometry& m_baseSceneObjectGeometry;
        const unsigned m_primitiveID;
    }

    class BotLevelLeafNode {
    public:
        BotLevelLeafNode(const OOCGeometricSceneObject& sceneObject, const SceneObjectGeometry& sceneObjectGeometry, unsigned primitiveID);
        BotLevelLeafNode(const OOCInstancedSceneObject& sceneObject, const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>& bvh);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        struct Regular {
            const OOCGeometricSceneObject& sceneObject;
            const SceneObjectGeometry& sceneObjectGeometry;
            const unsigned primitiveID;
        };

        struct Instance {
            const OOCInstancedSceneObject& sceneObject;
            std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>> bvh;
        };
        std::variant<Regular, Instance> m_data;
    };

    class TopLevelLeafNode {
    public:
        struct GeometryData {
            WiVeBVH8<BotLevelLeafNode> leafBVH;
            std::vector<std::unique_ptr<SceneObjectGeometry>> geometry;
        };

        TopLevelLeafNode(
            std::string_view cacheFilename,
            gsl::span<const OOCSceneObject*> sceneObjects,
            FifoCache<GeometryData> geometryCache,
            OOCBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure);
        TopLevelLeafNode(TopLevelLeafNode&& other);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        void prepareForFlushUnsafe(); // Adds the active batches to the list of immutable batches (even if they are not full)
        size_t flush(); // Flushes the list of immutable batches (but not the active batches)

    private:
        static EvictableResourceHandle<GeometryData> generateCachedBVH(std::string_view filename, gsl::span<const OOCSceneObject*> sceneObjects, FifoCache<GeometryData>& cache);

    private:
        EvictableResourceHandle<GeometryData> m_geometryDataHandle;
        std::vector<const OOCSceneObject*> m_sceneObjects;

        tbb::enumerable_thread_specific<RayBatch*> m_threadLocalActiveBatch;
        std::atomic<RayBatch*> m_immutableRayBatchList;
        OOCBatchingAccelerationStructure<UserState, BatchSize>& m_accelerationStructurePtr;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(gsl::span<const std::unique_ptr<OOCSceneObject>>, OOCBatchingAccelerationStructure& accelerationStructure);

private:
    GrowingFreeListTS<RayBatch> m_batchAllocator;
    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;
    tbb::enumerable_thread_specific<RayBatch*> m_threadLocalPreallocatedRaybatch;

    FifoCache<TopLevelLeafNode::GeometryData> m_geometryCache;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::OOCBatchingAccelerationStructure(gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects, HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_batchAllocator()
    , m_bvh(std::move(buildBVH(sceneObjects, *this)))
    , m_threadLocalPreallocatedRaybatch([&]() { return m_batchAllocator.allocate(); })
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
}

template <typename UserState, size_t BatchSize>
inline void OOCBatchingAccelerationStructure<UserState, BatchSize>::placeIntersectRequests(
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
            // If we get a result directly it must be because we missed the scene
            ALWAYS_ASSERT(*optResult == false);
            m_missCallback(ray, userState);
        }
    }
}

template <typename UserState, size_t BatchSize>
inline void OOCBatchingAccelerationStructure<UserState, BatchSize>::placeIntersectAnyRequests(
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
inline void OOCBatchingAccelerationStructure<UserState, BatchSize>::flush()
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
inline PauseableBVH4<typename OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> OOCBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects,
    OOCBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
{
    // Find all referenced geometric scene objects
    std::unordered_set<const OOCGeometricSceneObject*> uniqueGeometricSceneObjects;
    for (const auto& sceneObject : sceneObjects) {
        if (auto instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject.get())) {
            uniqueGeometricSceneObjects.insert(instancedSceneObject->getBaseObject());
        } else if (auto geometricSceneObject = dynamic_cast<const OOCGeometricSceneObject*>(sceneObject.get())) {
            uniqueGeometricSceneObjects.insert(geometricSceneObject);
        } else {
            THROW_ERROR("Unknown scene object type!");
        }
    }

    // Build a BVH for each unique GeometricSceneObject
    std::unordered_map<const OOCGeometricSceneObject*, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>> botLevelBVHs;
    for (auto sceneObjectPtr : uniqueGeometricSceneObjects) {
        std::vector<BotLevelLeafNode> leafs;
        for (unsigned primitiveID = 0; primitiveID < sceneObjectPtr->numPrimitives(); primitiveID++) {
            leafs.push_back(BotLevelLeafNode(sceneObjectPtr, primitiveID));
        }

        auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNode>>();
        bvh->build(leafs);
        botLevelBVHs[sceneObjectPtr] = bvh;
    }

    // Build the final BVH over all (instanced) scene objects
    std::vector<TopLevelLeafNode> leafs;
    for (const auto& sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject.get())) {
            auto bvh = botLevelBVHs[instancedSceneObject->getBaseObject()];
            if (bvh)
                leafs.emplace_back(sceneObject.get(), bvh, accelerationStructure);
        } else if (const auto* geometricSceneObject = dynamic_cast<const OOCGeometricSceneObject*>(sceneObject.get())) {
            auto bvh = botLevelBVHs[geometricSceneObject];
            if (bvh)
                leafs.emplace_back(sceneObject.get(), bvh, accelerationStructure);
        }
    }
    return PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    std::string_view cacheFilename,
    gsl::span<const OOCSceneObject*> sceneObjects,
    FifoCache<GeometryData> geometryCache,
    OOCBatchingAccelerationStructure<UserState, BatchSize>& accelerationStructure)
    : m_geometryDataCacheID(generateCachedBVH(cacheFilename, sceneObject, geometryCache))
    //, m_sceneObjects()
    , m_bounds(getGroupBounds(sceneObjects))
    , m_threadLocalActiveBatch([]() { return nullptr; })
    , m_immutableRayBatchList(nullptr)
    , m_accelerationStructurePtr(accelerationStructure)
{
    std::insert(std::end(m_sceneObjects), std::begin(sceneObjects), std::end(sceneObjects));
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_geometryDataCacheID(other.m_geometryDataCacheID)
    , m_sceneObjects(other.m_sceneObjects)
    , m_threadLocalActiveBatch(std::move(other.m_threadLocalActiveBatch))
    , m_immutableRayBatchList(other.m_immutableRayBatchList.load())
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
{
}

template <typename UserState, size_t BatchSize>
inline EvictableResourceHandle<OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::GeometryData> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::generateCachedBVH(
    std::string_view filename,
    gsl::span<const OOCSceneObject*> sceneObjects,
    FifoCache<GeometryData>& cache)
{
    // Collect the list of [unique] geometric scene objects that are referenced by instanced scene objects
    std::set<const OOCGeometricSceneObject*> instancedBaseObjects;
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObjectGeometry*>(geometry)) {
            instancedBaseObjects.insert(instancedBaseObjects->getBaseObject());
        }
    }

    flatbuffers::FlatBufferBuilder fbb;

    // Used for serialization to index into the list of instance base objects
    std::unordered_map<const OOCGeometricSceneObject*, uint32_t>> instanceBaseObjectIDs;
    std::vector<flatbuffers::Offset<serialization::GeometricSceneObjectGeometry>> serializedInstanceBaseGeometry;
    std::vector<flatbuffers::Offset<serialization::WiVeBVH8>> serializedInstanceBaseBVHs;

    // Build BVHs for instanced scene objects
    std::unordered_map<const OOCGeometricSceneObject*, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>> instancedBVHs;
    for (const auto* instancedGeometricSceneObject : instancedBaseObjects) {
        std::vector<BotLevelLeafNodeInstanced> leafs;

        auto geometry = instancedGeometricSceneObject->getGeometryBlocking();
        for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
            leafs.push_back(BotLevelLeafNodeInstanced(*geometry, primitiveID));
        }

        // NOTE: the "geometry" variable ensures that the geometry pointed to stays in memory for the BVH build
        //       (which requires the geometry to determine the leaf node bounds).
        auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>();
        bvh->build(leafs);
        instancedBVHs[instancedGeometricSceneObject] = bvh;

        instanceBaseObjectIDs[instancedGeometricSceneObject] = instanceBaseObjectID++;
        const auto* rawGeometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometry.get());
        instanceBaseObjectIDs[instancedGeometricSceneObject] = static_cast<uint32_t>(instanceBaseGeometry.size());
        serializedInstanceBaseGeometry.push_back(rawGeometry->serialize(fbb));
        serializedInstanceBaseBVHs.push_back(bvh->serialize(fbb));
    }

    std::vector<serialization::GeometricSceneObjectGeometry> serializedUniqueGeometry;
    std::vector<serialization::InstancedSceneObjectGeometry> serializedInstancedGeometry;

    // Create leaf nodes for all instanced geometry and then for all non-instanced geometry. It is important to keep these
    // two separate so we can recreate the leafs in the exact same order when deserializing the BVH.
    std::vector<BotLevelLeafNode> leafs;
    std::vector<const OOCSceneObject*> geometricSceneObjects;
    std::vector<const OOCSceneObject*> instancedSceneObjects;
    std::vector<unsigned> serializedInstancedSceneObjectBaseIDs;
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* geometricSceneObject = dynamic_cast<const OOCGeometricSceneObject*>(sceneObject)) {
            // Serialize
            const auto geometryOwningPointer = geometricSceneObject->getGeometryBlocking();
            const auto* geometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometryOwningPointer);
            serializedUniqueGeometry.push_back(
                geometry->serialize(fbb));
            geometricSceneObjects.push_back(sceneObject);

            // Create bot-level leaf node
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNode(sceneObject, *geometry, primitiveID));
            }
        }
    }
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const InstancedSceneObjectGeometry*>(geometry)) {
            // Serialize
            const auto geometryOwningPointer = instancedSceneObject->getGeometryBlocking();
            const auto* geometry = dynamic_cast<const InstancedSceneObjectGeometry*>(geometryOwningPointer);
            unsigned baseGeometryID = instanceBaseObjectIDs[instancedSceneObject->getBaseObject()];
            serializedInstancedSceneObjectBaseIDs.push_back(baseGeometryID);
            serializedInstancedGeometry.push_back(
                geometry->serialize(fbb, baseGeometryID));
            instancedSceneObjects.push_back(sceneObject);

            // Create bot-level node
            auto bvh = instancedBVHs[instancedSceneObject->getBaseObject()];
            leafs.push_back(BotLevelLeafNode(instancedSceneObject.get(), bvh));
        }
    }

    WiVeBVH8Build8<BotLevelLeafNode> bvh;
    bvh.build(leafs);
    auto serializedBVH = bvh.serialize(fbb);

    auto serializedTopLevelLeafNode = serialization::CreateOOCBatchingTopLevelLeafNode(
        fbb,
        fbb.CreateVector(serializedUniqueGeometry),
        fbb.CreateVector(serializedInstanceBaseGeometry),
        fbb.CreateVector(serializedInstanceBaseBVHs),
        fbb.CreateVector(serializedInstancedSceneObjectBaseIDs),
        fbb.CreateVector(serializedInstancedGeometry),
        serializedBVH);
    fbb.Finish(serializedTopLevelLeafNode);

    std::ofstream file;
    file.open(filename.data(), std::ios::out | std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), builder.GetSize());
    file.close();

    auto resourceID = cache.emplaceFactoryUnsafe([filename = std::string(filename), geometricSceneObjects, instancedSceneObjects]() -> GeometryData {
        auto mmapFile = moi::mmap_source(filename, 0, mio::map_entire_file);
        auto serializedTopLevelLeafNode = serialization::GetOOCBatchingTopLevelLeafNode(mmapFile.data());
        const auto* serializedInstanceBaseBVHs = serializedTopLevelLeafNode->instance_base_bvh();
        const auto* serializedInstanceBaseGeometry = serializedTopLevelLeafNode->instance_base_geometry();

        // Load geometry/BVH of geometric nodes that are referenced by instancing nodes
        std::vector < std::pair<std::unique_ptr<GeometricSceneObjectGeometry>, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNode>>> instanceBaseObjects;
        for (int i = 0; i < serializedInstanceBaseBVHs->size(); i++) {
            auto geometry = std::unique_ptr<GeometricSceneObjectGeometry>(serializedInstanceBaseGeometry->Get(i));

            std::vector<BotLevelLeafNodeInstanced> leafs;
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNodeInstanced(*geometry, primitiveID));
            }

            auto bvh = std::make_shared<WiVeBVH8>(serializedInstanceBaseBVHs->Get(i), leafs);
            instanceBaseObjects.push_back({ geometry, bvh });
        }

        GeometryData ret;

        const auto* serializedUniqueGeometry = serializedTopLevelLeafNode->unique_geometry();
        const auto* serializedInstancedGeometry = serializedTopLevelLeafNode->instanced_geometry();
        std::vector<BotLevelLeafNode> leafs;
        for (size_t i = 0; i < geometricSceneObjects.size(); i++) {
            auto geometry = std::make_unique<GeometricSceneObjectGeometry>(serializedUniqueGeometry->Get(i));

            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNode(*geometricSceneObjects[i], *geometry, primitiveID));
            }

            ret.geometry.emplace_back(std::move(geometry));
        }

        const auto* serializedInstancedIDs = serializedTopLevelLeafNode->instanced_ids();
        for (size_t i = 0; i < instancedSceneObjects->size(); i++) {
            auto geometryBaseID = serializedInstancedIDs->Get(i);
            auto [baseGeometry, bvh] = instanceBaseObjects[geometryBaseID];

            auto geometry = std::make_unique<InstancedSceneObjectGeometry>(
                serializedInstancedGeometry->Get(i),
                std::make_unique<GeometricSceneObjectGeometry>(baseGeometry));
            leafs.push_back(BotLevelLeafNode(*instancedSceneObjects[i], bvh));

            ret.geometry.emplace_back(std::move(geometry));
        }

        ret.leafBVH = WiVeBVH8(serializedTopLevelLeafNode->bvh());
        return ret;
    });
    return EvictableResourceHandle<GeometryData>(cache, resourceID);
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::getBounds() const
{
    Bounds ret;
    for (const auto* sceneObject : sceneObjects) {
        ret.extend(sceneObject->worldBounds());
    }
    return ret;
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(Ray& ray, RayHit& rayInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

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
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

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
}

template <typename UserState, size_t BatchSize>
inline void OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::prepareForFlushUnsafe()
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
inline size_t OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flush()
{
    RayBatch* batch = m_immutableRayBatchList.exchange(nullptr);
    if (!batch)
        return false;

    std::shared_ptr<GeometryData> data = m_geometryDataHandle.getBlocking();

    size_t numRays = 0;
    tbb::task_group taskGroup;
    while (batch) {
        numRays += batch->size();
        auto next = batch->next();

        taskGroup.run([=]() {
            for (auto& [ray, hitInfo, userState, insertHandle] : *batch) {
                // Intersect with the bottom-level BVH
                if (hitInfo) {
                    m_leafBVH->intersect(ray, *hitInfo);
                } else {
                    m_leafBVH->intersectAny(ray);
                }
            }

            for (auto [ray, hitInfo, userState, insertHandle] : *batch) {
                if (hitInfo) {
                    // Insert the ray back into the top-level  BVH
                    auto optResult = m_accelerationStructurePtr->m_bvh.intersect(ray, *hitInfo, userState, insertHandle);
                    if (optResult && *optResult == false) {
                        // Ray exited the system so hitInfo contains the closest hit
                        const auto& hitInfoSceneObject = std::get<RayHit::OutOfCore>(hitInfo->sceneObjectVariant);
                        if (hitSceneObject) {
                            // Compute the full surface interaction
                            SurfaceInteraction si = hitInfoSceneObject.sceneObjectGeometry->fillSurfaceInteraction(ray, *hitInfo);
                            si.sceneObjectMaterial = hitInfoSceneObject.sceneObject->lockMaterial();
                            m_accelerationStructurePtr->m_hitCallback(ray, si, userState, nullptr);
                        } else {
                            m_accelerationStructurePtr->m_missCallback(ray, userState);
                        }
                    }
                } else {
                    // Intersect any
                    if (ray.tfar == -std::numeric_limits<float>::infinity()) { // Ray hit something
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
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::BotLevelLeafNodeInstanced(
    const SceneObjectGeometry& baseSceneObjectGeometry,
    unsigned primitiveID)
    : m_baseSceneObject(baseSceneObject)
    , m_baseSceneObjectGeometry(baseSceneObjectGeometry)
    , m_primitiveID(primitiveID)
{
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::getBounds() const
{
    return m_baseSceneObjectGeometry.worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::intersect(Ray& ray, RayHit& hitInfo) const
{
    return m_baseSceneObjectGeometry.intersectPrimitive(ray, hitInfo, m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCGeometricSceneObject& sceneObject,
    const SceneObjectGeometry& sceneObjectGeometry,
    unsigned primitiveID)
    : m_data(Regular { sceneObject, sceneObjectGeometry, primitiveID })
{
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCInstancedSceneObject& sceneObject,
    const std::shared_ptr<WiVeBVH8Build8<OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced>>& bvh)
    : m_data(Instanced { sceneObject, bvh })
{
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::getBounds() const
{
    return m_sceneObject->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    if (std::holds_alternative<Regular>(m_data)) {
        const auto& data = std::get<Regular>(m_data);

        bool hit = data.sceneObjectGeometry.intersectPrimitive(ray, hitInfo, data.primitiveID);
        if (hit) {
            std::get<const OOCSceneObject*>(hitInfo.sceneObjectVariant) = &data.sceneObject;
        }
        return hit;
    } else {
        const auto& data = std::get<Instanced>(m_data);

        Ray localRay = data.sceneObject.transformRayToInstanceSpace(ray);

        bool hit = data.bvh.intersect(localRay, hitInfo);
        if (hit) {
            std::get<const OOCSceneObject*>(hitInfo.sceneObjectVariant) = &data.sceneObject;
        }

        ray.tfar = localRay.tfar;
        return hit;
    }
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const RayHit& hitInfo, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, hitInfo, state, insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    std::optional<RayHit> opt = {};
    m_data.emplace_back(ray, opt, state, insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline const typename OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::begin()
{
    return iterator(this, 0);
}

template <typename UserState, size_t BatchSize>
inline const typename OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::end()
{
    return iterator(this, m_data.size());
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::iterator(OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch* batch, size_t index)
    : m_rayBatch(batch)
    , m_index(index)
{
}

template <typename UserState, size_t BatchSize>
inline typename OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator& OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++()
{
    m_index++;
    return *this;
}

template <typename UserState, size_t BatchSize>
inline typename OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator++(int)
{
    auto r = *this;
    m_index++;
    return r;
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator==(OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index == other.m_index;
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator!=(OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator other) const
{
    assert(m_rayBatch == other.m_rayBatch);
    return m_index != other.m_index;
}

template <typename UserState, size_t BatchSize>
inline typename OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::value_type OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::iterator::operator*()
{
    auto& [ray, hitInfo, userState, insertHandle] = m_rayBatch->m_data[m_index];
    return { ray, hitInfo, userState, insertHandle };
}
}
