#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/core/stats.h"
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
#include <tbb/flow_graph.h>
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
    OOCBatchingAccelerationStructure(
        size_t geometryCacheSize,
        const Scene& scene,
        HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
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

        bool tryPush(const Ray& ray, const SurfaceInteraction& si, const UserState& state, const PauseableBVHInsertHandle& insertHandle);
        bool tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle);

        size_t size() const
        {
            return m_data.size();
        }

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray&, std::optional<SurfaceInteraction>&, UserState&, PauseableBVHInsertHandle&>;
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
            BatchItem(const Ray& ray, const UserState& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , si({})
                , userState(userState)
                , insertHandle(insertHandle)
            {
            }
            BatchItem(const Ray& ray, const SurfaceInteraction& si, const UserState& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , si(si)
                , userState(userState)
                , insertHandle(insertHandle)
            {
            }
            Ray ray;
            std::optional<SurfaceInteraction> si;
            UserState userState;
            PauseableBVHInsertHandle insertHandle;
        };
        eastl::fixed_vector<BatchItem, BatchSize> m_data;

        RayBatch* m_nextPtr;
    };

    class BotLevelLeafNodeInstanced {
    public:
        BotLevelLeafNodeInstanced(const SceneObjectGeometry* baseSceneObjectGeometry, unsigned primitiveID);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        const SceneObjectGeometry* m_baseSceneObjectGeometry;
        const unsigned m_primitiveID;
    };

    class BotLevelLeafNode {
    public:
        BotLevelLeafNode(const OOCGeometricSceneObject* sceneObject, const SceneObjectGeometry* sceneObjectGeometry, unsigned primitiveID);
        BotLevelLeafNode(const OOCInstancedSceneObject* sceneObject, const SceneObjectGeometry* sceneObjectGeometry, const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>& bvh);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        struct Regular {

            const OOCGeometricSceneObject* sceneObject;
            const SceneObjectGeometry* sceneObjectGeometry;
            const unsigned primitiveID;
        };

        struct Instance {
            const OOCInstancedSceneObject* sceneObject;
            const SceneObjectGeometry* sceneObjectGeometry;
            std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>> bvh;
        };
        std::variant<Regular, Instance> m_data;
    };

    class TopLevelLeafNode {
    public:
        struct GeometryData {
            size_t sizeBytes() const
            {
                size_t size = sizeof(decltype(*this));
                size += leafBVH.sizeBytes();
                size += geometrySize;
                return size;
            }

            size_t geometrySize = 0; // Simply iterating over geometryOwningPointers is incorrect because we would count instanced geometry multiple times
            WiVeBVH8Build8<BotLevelLeafNode> leafBVH;
            std::vector<std::unique_ptr<SceneObjectGeometry>> geometryOwningPointers;
        };

        TopLevelLeafNode(
            std::string_view cacheFilename,
            gsl::span<const OOCSceneObject*> sceneObjects,
            FifoCache<GeometryData>* geometryCache,
            OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);
        TopLevelLeafNode(TopLevelLeafNode&& other);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        void prepareForFlushUnsafe(); // Adds the active batches to the list of immutable batches (even if they are not full)

        //size_t flush(); // Flushes the list of immutable batches (but not the active batches)

        // Flush a whole range of nodes at a time as opposed to a non-static flush member function which would require a
        // separate tbb flow graph for each node that is processed.
        static size_t flushRange(
            gsl::span<TopLevelLeafNode*> nodes,
            OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

    private:
        static EvictableResourceID generateCachedBVH(
            std::string_view filename,
            gsl::span<const OOCSceneObject*> sceneObjects,
            FifoCache<GeometryData>* cache);

    private:
        EvictableResourceID m_geometryDataCacheID;
        std::vector<const OOCSceneObject*> m_sceneObjects;

        tbb::enumerable_thread_specific<RayBatch*> m_threadLocalActiveBatch;
        std::atomic<RayBatch*> m_immutableRayBatchList;
        OOCBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(
        std::string_view cacheFolder,
        FifoCache<typename TopLevelLeafNode::GeometryData>* cache,
        gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects,
        OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

private:
    FifoCache<typename TopLevelLeafNode::GeometryData> m_geometryCache;

    GrowingFreeListTS<RayBatch> m_batchAllocator;
    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;
    tbb::enumerable_thread_specific<RayBatch*> m_threadLocalPreallocatedRaybatch;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::OOCBatchingAccelerationStructure(
    size_t geometryCacheSize,
    const Scene& scene,
    HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_geometryCache(
          geometryCacheSize,
          [](size_t bytes) { g_stats.memory.botLevelLoaded += bytes; },
          [](size_t bytes) { g_stats.memory.botLevelEvicted += bytes; })
    , m_batchAllocator()
    , m_bvh(std::move(buildBVH("ooc_node_cache/", &m_geometryCache, scene.getOOCSceneObjects(), this)))
    , m_threadLocalPreallocatedRaybatch([&]() { return m_batchAllocator.allocate(); })
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
    // Clean the scenes geometry cache because it won't be used anymore. The batches recreate the geometry
    // and use their own cache to manage it.
    scene.geometryCache()->evictAllUnsafe();

    g_stats.memory.topBVH += m_bvh.sizeBytes();
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
        SurfaceInteraction si;
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        auto optResult = m_bvh.intersect(ray, si, userState);
        if (optResult && *optResult == false) {
            // If we get a result directly it must be because we missed the scene
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
/*#ifndef NDEBUG
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
#else*/
        tbb::parallel_for_each(m_bvh.leafs(), [](auto* topLevelLeafNode) {
            topLevelLeafNode->prepareForFlushUnsafe();
        });

        // Sort nodes so the ones with the most batches get processed first
        //tbb::parallel_sort(leafsWithBatchedRays, [](const TopLevelLeafNode* a, const TopLevelLeafNode* b) -> bool { return a->approximateBatchCount() < b->approximateBatchCount(); });

        /*tbb::enumerable_thread_specific<size_t> raysProcessedTL([]() -> size_t { return 0; });
        tbb::parallel_for_each(m_bvh.leafs(), [&](auto* topLevelLeafNode) {
            raysProcessedTL.local() += topLevelLeafNode->flush();
        });
        size_t raysProcessed = std::accumulate(std::begin(raysProcessedTL), std::end(raysProcessedTL), (size_t)0, std::plus<size_t>());
        if (raysProcessed == 0)
            break;*/

        size_t raysProcessed = TopLevelLeafNode::flushRange(m_bvh.leafs(), this);
        if (raysProcessed == 0)
            break;
//#endif
    }

    std::cout << "FLUSH COMPLETE" << std::endl;
}

template <typename UserState, size_t BatchSize>
inline PauseableBVH4<typename OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> OOCBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    std::string_view cacheFolder,
    FifoCache<typename TopLevelLeafNode::GeometryData>* cache,
    gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr)
{
    auto sceneObjectGroups = groupSceneObjects(25000, sceneObjects);


    std::vector<TopLevelLeafNode> leafs;
    for (size_t i = 0; i < sceneObjectGroups.size(); i++) {
        std::string cacheFilename = std::string(cacheFolder) + "node" + std::to_string(i) + ".bin";
        auto& group = sceneObjectGroups[i];
        leafs.emplace_back(cacheFilename, group, cache, accelerationStructurePtr);
    }

    std::cout << "Creating PauseableBVH4" << std::endl;
    return PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    std::string_view cacheFilename,
    gsl::span<const OOCSceneObject*> sceneObjects,
    FifoCache<GeometryData>* geometryCache,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructure)
    : m_geometryDataCacheID(generateCachedBVH(cacheFilename, sceneObjects, geometryCache))
    //, m_sceneObjects()
    , m_threadLocalActiveBatch([]() { return nullptr; })
    , m_immutableRayBatchList(nullptr)
    , m_accelerationStructurePtr(accelerationStructure)
{
    m_sceneObjects.insert(std::end(m_sceneObjects), std::begin(sceneObjects), std::end(sceneObjects));
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_geometryDataCacheID(other.m_geometryDataCacheID)
    , m_sceneObjects(std::move(other.m_sceneObjects))
    , m_threadLocalActiveBatch(std::move(other.m_threadLocalActiveBatch))
    , m_immutableRayBatchList(other.m_immutableRayBatchList.load())
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
{
}

template <typename UserState, size_t BatchSize>
inline EvictableResourceID OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::generateCachedBVH(
    std::string_view filename,
    gsl::span<const OOCSceneObject*> sceneObjects,
    FifoCache<GeometryData>* cache)
{
    // Collect the list of [unique] geometric scene objects that are referenced by instanced scene objects
    std::set<const OOCGeometricSceneObject*> instancedBaseObjects;
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject)) {
            instancedBaseObjects.insert(instancedSceneObject->getBaseObject());
        }
    }

    flatbuffers::FlatBufferBuilder fbb;

    // Used for serialization to index into the list of instance base objects
    std::unordered_map<const OOCGeometricSceneObject*, uint32_t> instanceBaseObjectIDs;
    std::vector<flatbuffers::Offset<serialization::GeometricSceneObjectGeometry>> serializedInstanceBaseGeometry;
    std::vector<flatbuffers::Offset<serialization::WiVeBVH8>> serializedInstanceBaseBVHs;

    // Build BVHs for instanced scene objects
    std::unordered_map<const OOCGeometricSceneObject*, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>> instancedBVHs;
    for (const auto* instancedGeometricSceneObject : instancedBaseObjects) {
        auto geometry = instancedGeometricSceneObject->getGeometryBlocking();

        std::vector<BotLevelLeafNodeInstanced> leafs;
        leafs.reserve(geometry->numPrimitives());
        for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
            leafs.push_back(BotLevelLeafNodeInstanced(geometry.get(), primitiveID));
        }
        std::cout << "leafs size: " << leafs.size() << std::endl;

        // NOTE: the "geometry" variable ensures that the geometry pointed to stays in memory for the BVH build
        //       (which requires the geometry to determine the leaf node bounds).
        auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>();
        bvh->build(leafs);
        instancedBVHs[instancedGeometricSceneObject] = bvh;

        const auto* rawGeometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometry.get());
        instanceBaseObjectIDs[instancedGeometricSceneObject] = static_cast<uint32_t>(serializedInstanceBaseGeometry.size());
        serializedInstanceBaseGeometry.push_back(rawGeometry->serialize(fbb));
        serializedInstanceBaseBVHs.push_back(bvh->serialize(fbb));
    }

    std::vector<flatbuffers::Offset<serialization::GeometricSceneObjectGeometry>> serializedUniqueGeometry;
    std::vector<flatbuffers::Offset<serialization::InstancedSceneObjectGeometry>> serializedInstancedGeometry;

    std::vector<std::unique_ptr<SceneObjectGeometry>> geometryOwningPointers; // Keep geometry alive until BVH build finished

    // Create leaf nodes for all instanced geometry and then for all non-instanced geometry. It is important to keep these
    // two separate so we can recreate the leafs in the exact same order when deserializing the BVH.
    std::vector<BotLevelLeafNode> leafs;
    std::vector<const OOCGeometricSceneObject*> geometricSceneObjects;
    std::vector<const OOCInstancedSceneObject*> instancedSceneObjects;
    std::vector<unsigned> serializedInstancedSceneObjectBaseIDs;
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* geometricSceneObject = dynamic_cast<const OOCGeometricSceneObject*>(sceneObject)) {
            // Serialize
            auto geometryOwningPointer = geometricSceneObject->getGeometryBlocking();
            const auto* geometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometryOwningPointer.get());
            serializedUniqueGeometry.push_back(
                geometry->serialize(fbb));
            geometricSceneObjects.push_back(geometricSceneObject);

            // Create bot-level leaf node
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNode(geometricSceneObject, geometry, primitiveID));
            }

            geometryOwningPointers.emplace_back(std::move(geometryOwningPointer)); // Keep geometry alive until BVH build finished
        }
    }
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject)) {
            // Serialize
            auto geometryOwningPointer = instancedSceneObject->getGeometryBlocking();
            const auto* geometry = dynamic_cast<const InstancedSceneObjectGeometry*>(geometryOwningPointer.get());
            unsigned baseGeometryID = instanceBaseObjectIDs[instancedSceneObject->getBaseObject()];
            serializedInstancedSceneObjectBaseIDs.push_back(baseGeometryID);
            serializedInstancedGeometry.push_back(
                geometry->serialize(fbb));
            instancedSceneObjects.push_back(instancedSceneObject);

            // Create bot-level node
            auto bvh = instancedBVHs[instancedSceneObject->getBaseObject()];
            leafs.push_back(BotLevelLeafNode(instancedSceneObject, geometry, bvh));

            geometryOwningPointers.emplace_back(std::move(geometryOwningPointer));
        }
    }

    std::cout << "TopLevelLeafNode with " << sceneObjects.size() << " objects" << std::endl;

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
    file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    file.close();

    auto resourceID = cache->emplaceFactoryUnsafe([filename = std::string(filename), geometricSceneObjects = std::move(geometricSceneObjects), instancedSceneObjects=std::move(instancedSceneObjects)]() -> GeometryData {
        auto mmapFile = mio::mmap_source(filename, 0, mio::map_entire_file);
        auto serializedTopLevelLeafNode = serialization::GetOOCBatchingTopLevelLeafNode(mmapFile.data());

        /*std::ifstream file(filename, std::ios::binary | std::ios::ate);
        assert(file.is_open());
        auto pos = file.tellg();
        
        auto chars = std::make_unique<char[]>(pos);
        file.seekg(0, std::ios::beg);
        file.read(chars.get(), pos);
        file.close();

        auto serializedTopLevelLeafNode = serialization::GetOOCBatchingTopLevelLeafNode(chars.get());*/

        const auto* serializedInstanceBaseBVHs = serializedTopLevelLeafNode->instance_base_bvh();
        const auto* serializedInstanceBaseGeometry = serializedTopLevelLeafNode->instance_base_geometry();

        GeometryData ret;

        // Load geometry/BVH of geometric nodes that are referenced by instancing nodes
        std::vector<std::pair<const GeometricSceneObjectGeometry*, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>>> instanceBaseObjects;
        for (unsigned i = 0; i < serializedInstanceBaseBVHs->size(); i++) {
            auto geometry = std::make_unique<GeometricSceneObjectGeometry>(serializedInstanceBaseGeometry->Get(i));

            std::vector<BotLevelLeafNodeInstanced> leafs;
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNodeInstanced(geometry.get(), primitiveID));
            }

            auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>(serializedInstanceBaseBVHs->Get(i), std::move(leafs));
            instanceBaseObjects.push_back({ geometry.get(), bvh });

            // The BVH leaf nodes point directly to geometry so we should keep them alive (they point to the same mesh though)
            ret.geometrySize += geometry->sizeBytes();
            ret.geometryOwningPointers.emplace_back(std::move(geometry));
        }

        // Load unique geometry
        const auto* serializedUniqueGeometry = serializedTopLevelLeafNode->unique_geometry();
        const auto* serializedInstancedGeometry = serializedTopLevelLeafNode->instanced_geometry();
        std::vector<BotLevelLeafNode> leafs;
        for (unsigned i = 0; i < geometricSceneObjects.size(); i++) {
            auto geometry = std::make_unique<GeometricSceneObjectGeometry>(serializedUniqueGeometry->Get(i));

            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.emplace_back(geometricSceneObjects[i], geometry.get(), primitiveID);
            }

            ret.geometrySize += geometry->sizeBytes();
            ret.geometryOwningPointers.emplace_back(std::move(geometry));
        }

        // Load instanced geometry
        const auto* serializedInstancedIDs = serializedTopLevelLeafNode->instanced_ids();
        for (unsigned i = 0; i < instancedSceneObjects.size(); i++) {
            auto geometryBaseID = serializedInstancedIDs->Get(i);
            const auto& [baseGeometry, baseBVH] = instanceBaseObjects[geometryBaseID];

            auto geometry = std::make_unique<InstancedSceneObjectGeometry>(
                serializedInstancedGeometry->Get(i),
                std::make_unique<GeometricSceneObjectGeometry>(*baseGeometry));
            leafs.push_back(BotLevelLeafNode(instancedSceneObjects[i], geometry.get(), baseBVH));

            ret.geometryOwningPointers.emplace_back(std::move(geometry));
        }

        ret.leafBVH = WiVeBVH8Build8<BotLevelLeafNode>(serializedTopLevelLeafNode->bvh(), std::move(leafs));
        return ret;
    });
    return resourceID;
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::getBounds() const
{
    Bounds ret;
    for (const auto* sceneObject : m_sceneObjects) {
        ret.extend(sceneObject->worldBounds());
    }
    return ret;
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(
    Ray& ray,
    SurfaceInteraction& si,
    const UserState& userState,
    PauseableBVHInsertHandle insertHandle) const
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

    bool success = batch->tryPush(ray, si, userState, insertHandle);
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
size_t OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flushRange(
    gsl::span<TopLevelLeafNode*> nodes,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr)
{
    tbb::flow::graph g;

    // Generate a task for each top-level leaf node with at least one non-empty batch
    int i = 0;
    tbb::flow::source_node<std::pair<RayBatch*, EvictableResourceID>> sourceNode(
        g,
        [&](std::pair<RayBatch*, EvictableResourceID>& out) -> bool {
            while (i < nodes.size()) {
                TopLevelLeafNode* node = nodes[i++];
                RayBatch* batch = node->m_immutableRayBatchList.exchange(nullptr);
                if (!batch)
                    continue;

                out = { batch, node->m_geometryDataCacheID };
                return true;
            }

            return (i < nodes.size());
        });

    // For each of those leaf nodes, load the geometry (asynchronously)
    auto cacheSubGraph = std::move(accelerationStructurePtr->m_geometryCache.template getFlowGraphNode<RayBatch*>(g));

    // Then create a task for each batch associated with that leaf node (for increased parallelism)
    using BatchWithGeom = std::pair<RayBatch*, std::shared_ptr<GeometryData>>;
    using BatchNodeType = tbb::flow::multifunction_node<BatchWithGeom, tbb::flow::tuple<BatchWithGeom>>;
    BatchNodeType batchNode(
        g,
        tbb::flow::unlimited,
        [](const BatchWithGeom& v, typename BatchNodeType::output_ports_type& op) {
            auto* batch = std::get<0>(v);

            size_t raysProcessed = 0;
            while (batch) {
                std::get<0>(op).try_put(std::make_pair(batch, std::get<1>(v)));
                batch = batch->next();
            }
        });

    tbb::flow::function_node<BatchWithGeom, size_t> traversalNode(g, tbb::flow::unlimited, [&](const BatchWithGeom& v) {
        auto* batch = std::get<0>(v);
        auto geometryData = std::get<1>(v);

        for (auto [ray, siOpt, userState, insertHandle] : *batch) {
            // Intersect with the bottom-level BVH
            if (siOpt) {
                RayHit rayHit;
                if (geometryData->leafBVH.intersect(ray, rayHit)) {
                    const auto& hitSceneObjectInfo = std::get<RayHit::OutOfCore>(rayHit.sceneObjectVariant);
                    siOpt = hitSceneObjectInfo.sceneObjectGeometry->fillSurfaceInteraction(ray, rayHit);
                    siOpt->sceneObject = hitSceneObjectInfo.sceneObject;
                }
            } else {
                geometryData->leafBVH.intersectAny(ray);
            }
        }

        for (auto [ray, siOpt, userState, insertHandle] : *batch) {
            if (siOpt) {
                // Insert the ray back into the top-level  BVH
                auto optResult = accelerationStructurePtr->m_bvh.intersect(ray, *siOpt, userState, insertHandle);
                if (optResult && *optResult == false) {
                    // Ray exited the system so we can run the hit/miss shaders
                    if (siOpt->sceneObject) {
                        auto materialOwning = siOpt->sceneObject->getMaterialBlocking(); // Keep alive during the callback
                        siOpt->sceneObjectMaterial = materialOwning.get();
                        accelerationStructurePtr->m_hitCallback(ray, *siOpt, userState, nullptr);
                    } else {
                        accelerationStructurePtr->m_missCallback(ray, userState);
                    }
                }
            } else {
                // Intersect any
                if (ray.tfar == -std::numeric_limits<float>::infinity()) { // Ray hit something
                    accelerationStructurePtr->m_anyHitCallback(ray, userState);
                } else {
                    auto optResult = accelerationStructurePtr->m_bvh.intersectAny(ray, userState, insertHandle);
                    if (optResult && *optResult == false) {
                        // Ray exited system
                        accelerationStructurePtr->m_missCallback(ray, userState);
                    }
                }
            }
        }
        auto raysProcessed = batch->size();
        accelerationStructurePtr->m_batchAllocator.deallocate(batch);
        return raysProcessed;
    });

    std::atomic_size_t totalRaysProcessed(0);
    tbb::flow::function_node<size_t, size_t> sumNode(g, tbb::flow::unlimited, [&](size_t raysProcessed) {
        return totalRaysProcessed.fetch_add(raysProcessed) + raysProcessed;
    });

    // NOTE: The source starts outputting as soon as an edge is connected.
    //       So make sure it is the last edge that we connect.
    tbb::flow::make_edge(traversalNode, sumNode);
    tbb::flow::make_edge(batchNode, traversalNode);
    cacheSubGraph.connectOutput(batchNode);
    cacheSubGraph.connectInput(sourceNode);
    //tbb::flow::make_edge(asyncCacheNode, batchNode);
    //tbb::flow::make_edge(sourceNode, asyncCacheNode);
    g.wait_for_all();

    return totalRaysProcessed;
}

/*template <typename UserState, size_t BatchSize>
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
            for (auto& [ray, siOpt, userState, insertHandle] : *batch) {
                // Intersect with the bottom-level BVH
                if (siOpt) {
                    RayHit rayHit;
                    if (data->leafBVH.intersect(ray, rayHit)) {
                        const auto& hitSceneObjectInfo = std::get<RayHit::OutOfCore>(rayHit.sceneObjectVariant);
                        siOpt = hitSceneObjectInfo.sceneObjectGeometry->fillSurfaceInteraction(ray, rayHit);
                        siOpt->sceneObject = hitSceneObjectInfo.sceneObject;
                    }
                } else {
                    data->leafBVH.intersectAny(ray);
                }
            }

            for (auto [ray, siOpt, userState, insertHandle] : *batch) {
                if (siOpt) {
                    // Insert the ray back into the top-level  BVH
                    auto optResult = m_accelerationStructurePtr->m_bvh.intersect(ray, *siOpt, userState, insertHandle);
                    if (optResult && *optResult == false) {
                        // Ray exited the system so we can run the hit/miss shaders
                        if (siOpt->sceneObject) {
                            auto materialOwning = siOpt->sceneObject->getMaterialBlocking(); // Keep alive during the callback
                            siOpt->sceneObjectMaterial = materialOwning.get();
                            m_accelerationStructurePtr->m_hitCallback(ray, *siOpt, userState, nullptr);
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
}*/

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::BotLevelLeafNodeInstanced(
    const SceneObjectGeometry* baseSceneObjectGeometry,
    unsigned primitiveID)
    : m_baseSceneObjectGeometry(baseSceneObjectGeometry)
    , m_primitiveID(primitiveID)
{
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::getBounds() const
{
    return m_baseSceneObjectGeometry->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::intersect(Ray& ray, RayHit& hitInfo) const
{
    auto ret = m_baseSceneObjectGeometry->intersectPrimitive(ray, hitInfo, m_primitiveID);
    return ret;
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCGeometricSceneObject* sceneObject,
    const SceneObjectGeometry* sceneObjectGeometry,
    unsigned primitiveID)
    : m_data(Regular { sceneObject, sceneObjectGeometry, primitiveID })
{
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCInstancedSceneObject* sceneObject,
    const SceneObjectGeometry* sceneObjectGeometry,
    const std::shared_ptr<WiVeBVH8Build8<OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced>>& bvh)
    : m_data(Instance { sceneObject, sceneObjectGeometry, bvh })
{
}

template <typename UserState, size_t BatchSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::getBounds() const
{
    if (std::holds_alternative<Regular>(m_data)) {
        const auto& data = std::get<Regular>(m_data);
        return data.sceneObjectGeometry->worldBoundsPrimitive(data.primitiveID);
    } else {
        return std::get<Instance>(m_data).sceneObject->worldBounds();
    }
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    if (std::holds_alternative<Regular>(m_data)) {
        const auto& data = std::get<Regular>(m_data);

        bool hit = data.sceneObjectGeometry->intersectPrimitive(ray, hitInfo, data.primitiveID);
        if (hit) {
            hitInfo.sceneObjectVariant = RayHit::OutOfCore { data.sceneObject, data.sceneObjectGeometry };
        }
        return hit;
    } else {
        const auto& data = std::get<Instance>(m_data);

        Ray localRay = data.sceneObject->transformRayToInstanceSpace(ray);
        bool hit = data.bvh->intersect(localRay, hitInfo);
        if (hit) {
            hitInfo.sceneObjectVariant = RayHit::OutOfCore { data.sceneObject, data.sceneObjectGeometry };
        }

        ray.tfar = localRay.tfar;
        return hit;
    }
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const SurfaceInteraction& si, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, si, state, insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, state, insertHandle);

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
    auto& [ray, si, userState, insertHandle] = m_rayBatch->m_data[m_index];
    return { ray, si, userState, insertHandle };
}
}
