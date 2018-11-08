#pragma once
#include "pandora/config.h"
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/core/stats.h"
#include "pandora/eviction/evictable.h"
#include "pandora/flatbuffers/ooc_batching2_generated.h"
#include "pandora/flatbuffers/ooc_batching_generated.h"
#include "pandora/geometry/triangle.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/file_batcher.h"
#include "pandora/utility/growing_free_list_ts.h"
#include <atomic>
#include <filesystem>
#include <gsl/gsl>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/flow_graph.h>
#include <tbb/mutex.h>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_sort.h>
#include <tbb/scalable_allocator.h>
#include <tbb/task_group.h>
#include <thread>
#include <unordered_set>

using namespace std::string_literals;

namespace pandora {

#ifdef D_OUT_OF_CORE_CACHE_FOLDER
// Dumb macro magic so I can move the cache folder to my slower SSD on my local machine, remove from the final build!!!
// https://stackoverflow.com/questions/6852920/how-do-i-turn-a-macro-into-a-string-using-cpp
#define QUOTE(x) #x
#define STR(x) QUOTE(x)
static const std::filesystem::path OUT_OF_CORE_CACHE_FOLDER(STR(D_OUT_OF_CORE_CACHE_FOLDER));
#else
static const std::filesystem::path OUT_OF_CORE_CACHE_FOLDER("ooc_node_cache/");
#endif

template <typename UserState, size_t BlockSize = 32>
class OOCBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    OOCBatchingAccelerationStructure(
        const Scene& scene,
        HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
    ~OOCBatchingAccelerationStructure() = default;

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);
    void placeIntersectAnyRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

    void flush();

private:
    struct RayBlock {
    public:
        RayBlock(RayBlock* nextPtr = nullptr)
            : m_nextPtr(nextPtr)
        {
        }
        void setNext(RayBlock* nextPtr) { m_nextPtr = nextPtr; }
        RayBlock* next() { return m_nextPtr; }
        const RayBlock* next() const { return m_nextPtr; }
        bool full() const { return m_data.full(); }

        bool tryPush(const Ray& ray, const RayHit& rayHit, const UserState& state, const PauseableBVHInsertHandle& insertHandle);
        bool tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle);

        size_t size() const
        {
            return m_data.size();
        }

        size_t sizeBytes() const
        {
            return sizeof(decltype(*this));
        }

        // https://www.fluentcpp.com/2018/05/08/std-iterator-deprecated/
        struct iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = std::tuple<Ray&, std::optional<RayHit>&, UserState&, PauseableBVHInsertHandle&>;
            using difference_type = std::ptrdiff_t;
            using pointer = value_type*;
            using reference = value_type&;

            explicit iterator(RayBlock* block, size_t index);
            iterator& operator++(); // pre-increment
            iterator operator++(int); // post-increment
            bool operator==(iterator other) const;
            bool operator!=(iterator other) const;
            value_type operator*();

        private:
            RayBlock* m_rayBlock;
            size_t m_index;
        };

        const iterator begin();
        const iterator end();

    private:
        // NOTE: using a std::array here is expensive because the default constructor of all items will be called when the block is created
        // The larger the blocks, the more expensive this becomes
        struct BlockItem {
            BlockItem(const Ray& ray, const UserState& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , hitInfo({})
                , userState(userState)
                , insertHandle(insertHandle)
            {
            }
            BlockItem(const Ray& ray, const RayHit& hitInfo, const UserState& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , hitInfo(hitInfo)
                , userState(userState)
                , insertHandle(insertHandle)
            {
            }
            Ray ray;
            std::optional<RayHit> hitInfo;
            UserState userState;
            PauseableBVHInsertHandle insertHandle;
        };
        eastl::fixed_vector<BlockItem, BlockSize> m_data;

        RayBlock* m_nextPtr;
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
        BotLevelLeafNode(const OOCGeometricSceneObject* sceneObject, const std::shared_ptr<SceneObjectGeometry>& sceneObjectGeometry, unsigned primitiveID);
        BotLevelLeafNode(const OOCInstancedSceneObject* sceneObject, const std::shared_ptr<SceneObjectGeometry>& sceneObjectGeometry, const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>& bvh);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        struct Regular {

            const OOCGeometricSceneObject* sceneObject;
            const std::shared_ptr<SceneObjectGeometry> sceneObjectGeometry;
            const unsigned primitiveID;
        };

        struct Instance {
            const OOCInstancedSceneObject* sceneObject;
            const std::shared_ptr<SceneObjectGeometry> sceneObjectGeometry;
            std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>> bvh;
        };
        std::variant<Regular, Instance> m_data;
    };

    //using CachedInstanceData = std::pair<std::shared_ptr<GeometricSceneObjectGeometry>, std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>>;
    struct CachedInstanceData {
        size_t sizeBytes() const
        {
            return sizeof(decltype(*this)) + geometry->sizeBytes() + bvh->sizeBytes();
        }

        std::shared_ptr<GeometricSceneObjectGeometry> geometry;
        std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>> bvh;
    };
    struct CachedBlockingPoint {
        size_t sizeBytes() const
        {
            size_t size = sizeof(decltype(*this));
            size += geometrySize;
            size += leafBVH.sizeBytes();
            size += geometryOwningPointers.size() * sizeof(typename decltype(geometryOwningPointers)::value_type);
            return size;
        }

        size_t geometrySize = 0; // Simply iterating over geometryOwningPointers is incorrect because we would count instanced geometry multiple times
        WiVeBVH8Build8<BotLevelLeafNode> leafBVH;
        std::vector<std::shared_ptr<SceneObjectGeometry>> geometryOwningPointers;
    };
    using MyCacheT = CacheT<CachedInstanceData, CachedBlockingPoint>;

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode(
            DiskDataBatcher& outputWriter,
            gsl::span<const OOCSceneObject*> sceneObjects,
            const std::unordered_map<const OOCGeometricSceneObject*, EvictableResourceHandle<CachedInstanceData, MyCacheT>>& serializedInstanceBaseSceneObject,
            MyCacheT* geometryCache,
            OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructurePtr);
        TopLevelLeafNode(TopLevelLeafNode&& other);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, RayHit& hitInfo, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Blocks rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Blocks rays. This function is thread safe.

        void forceLoad() { m_accelerationStructurePtr->m_geometryCache.template getBlocking<CachedBlockingPoint>(m_geometryDataCacheID); }
        bool inCache() const { return m_accelerationStructurePtr->m_geometryCache.inCache(m_geometryDataCacheID); }
        bool hasBatchedRays() { return m_immutableRayBlockList.load() != nullptr; }
        bool forwardPartiallyFilledBlocks(); // Adds the active blocks to the list of immutable blocks (even if they are not full)
        // Flush a whole range of nodes at a time as opposed to a non-static flush member function which would require a
        // separate tbb flow graph for each node that is processed.
        static void flushRange(
            gsl::span<TopLevelLeafNode*> nodes,
            OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructurePtr);

        static void compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes);

        size_t sizeBytes() const;

    private:
        static EvictableResourceID generateCachedBVH(
            DiskDataBatcher& outputWriter,
            gsl::span<const OOCSceneObject*> sceneObjects,
            const std::unordered_map<const OOCGeometricSceneObject*, EvictableResourceHandle<CachedInstanceData, MyCacheT>>& serializedInstanceBaseSceneObject,
            MyCacheT* cache);
        static Bounds computeBounds(gsl::span<const OOCSceneObject*> sceneObjects);

        struct SVDAGRayOffset {
            glm::vec3 gridBoundsMin;
            glm::vec3 invGridBoundsExtent;
        };
        static std::pair<SparseVoxelDAG, SVDAGRayOffset> computeSVDAG(gsl::span<const OOCSceneObject*> sceneObjects);

    private:
        const EvictableResourceID m_geometryDataCacheID;
        const Bounds m_bounds;

        std::atomic_int m_numFullBlocks;
        tbb::enumerable_thread_specific<RayBlock*> m_threadLocalActiveBlock;
        std::atomic<RayBlock*> m_immutableRayBlockList;
        OOCBatchingAccelerationStructure<UserState, BlockSize>* m_accelerationStructurePtr;

        std::pair<SparseVoxelDAG, SVDAGRayOffset> m_svdagAndTransform;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(
        MyCacheT* cache,
        const Scene& scene,
        OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructurePtr);

private:
    const int m_numLoadingThreads;
    MyCacheT m_geometryCache;

    GrowingFreeListTS<RayBlock> m_blockAllocator;
    //tbb::scalable_allocator<RayBlock> m_blockAllocator;

    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::OOCBatchingAccelerationStructure(
    const Scene& scene,
    HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_numLoadingThreads(2 * std::thread::hardware_concurrency())
    , m_geometryCache(
          OUT_OF_CORE_MEMORY_LIMIT,
          m_numLoadingThreads,
          [](size_t bytes) { g_stats.memory.botLevelLoaded += bytes; },
          [](size_t bytes) { g_stats.memory.botLevelEvicted += bytes; })
    , m_blockAllocator()
    , m_bvh(std::move(buildBVH(&m_geometryCache, scene, this)))
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
    // Clean the scenes geometry cache because it won't be used anymore. The blocks recreate the geometry
    // and use their own cache to manage it.
    scene.geometryCache()->evictAllUnsafe();

    g_stats.memory.topBVH += m_bvh.sizeBytes();
    for (const auto* leaf : m_bvh.leafs()) {
        g_stats.memory.topBVHLeafs += leaf->sizeBytes();
    }
}

template <typename UserState, size_t BlockSize>
inline void OOCBatchingAccelerationStructure<UserState, BlockSize>::placeIntersectRequests(
    gsl::span<const Ray> rays,
    gsl::span<const UserState> perRayUserData,
    const InsertHandle& insertHandle)
{
    (void)insertHandle;
    assert(perRayUserData.size() == rays.size());

    for (int i = 0; i < rays.size(); i++) {
        RayHit rayHit;
        Ray ray = rays[i]; // Copy so we can mutate it
        UserState userState = perRayUserData[i];

        auto optResult = m_bvh.intersect(ray, rayHit, userState);
        if (optResult && *optResult == false) {
            // If we get a result directly it must be because we missed the scene
            m_missCallback(ray, userState);
        }
    }
}

template <typename UserState, size_t BlockSize>
inline void OOCBatchingAccelerationStructure<UserState, BlockSize>::placeIntersectAnyRequests(
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
        if (optResult && *optResult == false) {
            // If we get a result directly it must be because we missed the scene
            m_missCallback(ray, userState);
        }
    }
}

template <typename UserState, size_t BlockSize>
inline PauseableBVH4<typename OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode, UserState> OOCBatchingAccelerationStructure<UserState, BlockSize>::buildBVH(
    MyCacheT* cache,
    const Scene& scene,
    OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructurePtr)
{
    if (!std::filesystem::exists(OUT_OF_CORE_CACHE_FOLDER)) {
        std::cout << "Create cache folder: " << OUT_OF_CORE_CACHE_FOLDER << std::endl;
        std::filesystem::create_directories(OUT_OF_CORE_CACHE_FOLDER);
    }
    ALWAYS_ASSERT(std::filesystem::is_directory(OUT_OF_CORE_CACHE_FOLDER));

    std::cout << "Building BVHs for all instanced geometry and storing them to disk\n";

    // Collect scene objects that are referenced by instanced scene objects.
    std::unordered_set<const OOCGeometricSceneObject*> instancedBaseSceneObjects;
    for (const auto* sceneObject : scene.getOOCSceneObjects()) {
        if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject)) {
            instancedBaseSceneObjects.insert(instancedSceneObject->getBaseObject());
        }
    }

    // For each of those objects build a BVH and store the object+BVH on disk
    DiskDataBatcher fileBlocker(OUT_OF_CORE_CACHE_FOLDER, "instanced_object", 500 * 1024 * 1024); // Block instanced geometry into files of 500MB
    std::unordered_map<const OOCGeometricSceneObject*, EvictableResourceHandle<CachedInstanceData, MyCacheT>> instancedBVHs;
    int instanceBaseFileNum = 0;
    for (const auto* instancedBaseSceneObject : instancedBaseSceneObjects) { // TODO: parallelize
        auto geometry = instancedBaseSceneObject->getGeometryBlocking();

        std::vector<BotLevelLeafNodeInstanced> leafs;
        leafs.reserve(geometry->numPrimitives());
        for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
            leafs.push_back(BotLevelLeafNodeInstanced(geometry.get(), primitiveID));
        }

        // NOTE: the "geometry" variable ensures that the geometry pointed to stays in memory for the BVH build
        //       (which requires the geometry to determine the leaf node bounds).
        WiVeBVH8Build8<BotLevelLeafNodeInstanced> bvh(leafs);

        // Serialize
        flatbuffers::FlatBufferBuilder fbb;
        const auto* rawGeometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometry.get());
        ALWAYS_ASSERT(rawGeometry != nullptr);
        auto serializedGeometry = rawGeometry->serialize(fbb);
        auto serializedBVH = bvh.serialize(fbb);
        auto serializedBaseSceneObject = serialization::CreateOOCBatchingBaseSceneObject(
            fbb,
            serializedGeometry,
            serializedBVH);
        fbb.Finish(serializedBaseSceneObject);

        // Write to disk
        auto filePart = fileBlocker.writeData(fbb);

        // Callback to restore the data we have just written to disk
        auto resourceID = cache->template emplaceFactoryThreadSafe<CachedInstanceData>([filePart]() -> CachedInstanceData {
            g_stats.memory.oocTotalDiskRead += filePart.size;
            auto buffer = filePart.load();
            auto serializedBaseSceneObject = serialization::GetOOCBatchingBaseSceneObject(buffer.data());
            auto geometry = std::make_shared<GeometricSceneObjectGeometry>(serializedBaseSceneObject->base_geometry());

            std::vector<BotLevelLeafNodeInstanced> leafs;
            leafs.reserve(geometry->numPrimitives());
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNodeInstanced(geometry.get(), primitiveID));
            }

            auto bvh = std::make_shared<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>(serializedBaseSceneObject->bvh(), std::move(leafs));
            return CachedInstanceData { geometry, bvh };
        });
        instancedBVHs.insert({ instancedBaseSceneObject, EvictableResourceHandle<CachedInstanceData, MyCacheT>(cache, resourceID) });
    }
    fileBlocker.flush();

    std::cout << "Creating scene object groups" << std::endl;
    auto sceneObjectGroups = scene.groupOOCSceneObjects(OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF);

    std::cout << "Creating leaf nodes" << std::endl;
    // Use disk batcher (for code reuse) with a file size limit of 0 so that it creates a new file every time.
    DiskDataBatcher nodeCacheDiskBatcher(OUT_OF_CORE_CACHE_FOLDER, "node", 0);
    std::mutex m;
    std::vector<TopLevelLeafNode> leafs;
    tbb::parallel_for(tbb::blocked_range<size_t>(0llu, sceneObjectGroups.size()), [&](tbb::blocked_range<size_t> localRange) {
        for (size_t i = localRange.begin(); i < localRange.end(); i++) {
            TopLevelLeafNode leaf(nodeCacheDiskBatcher, sceneObjectGroups[i], instancedBVHs, cache, accelerationStructurePtr);
            {
                std::scoped_lock<std::mutex> l(m);
                leafs.push_back(std::move(leaf));
            }
        }
    });

    g_stats.numTopLevelLeafNodes = static_cast<int>(leafs.size());

    std::cout << "Building top-level BVH" << std::endl;
    auto ret = PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
    TopLevelLeafNode::compressSVDAGs(ret.leafs());
    return std::move(ret);
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::TopLevelLeafNode(
    DiskDataBatcher& diskDataBatcher,
    gsl::span<const OOCSceneObject*> sceneObjects,
    const std::unordered_map<const OOCGeometricSceneObject*, EvictableResourceHandle<CachedInstanceData, MyCacheT>>& baseSceneObjectCacheHandles,
    MyCacheT* geometryCache,
    OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructure)
    : m_geometryDataCacheID(generateCachedBVH(diskDataBatcher, sceneObjects, baseSceneObjectCacheHandles, geometryCache))
    , m_bounds(computeBounds(sceneObjects))
    , m_numFullBlocks(0)
    , m_threadLocalActiveBlock([]() { return nullptr; })
    , m_immutableRayBlockList(nullptr)
    , m_accelerationStructurePtr(accelerationStructure)
    , m_svdagAndTransform(computeSVDAG(sceneObjects))
{
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_geometryDataCacheID(other.m_geometryDataCacheID)
    , m_bounds(std::move(other.m_bounds))
    , m_numFullBlocks(other.m_numFullBlocks.load())
    , m_threadLocalActiveBlock(std::move(other.m_threadLocalActiveBlock))
    , m_immutableRayBlockList(other.m_immutableRayBlockList.load())
    , m_accelerationStructurePtr(other.m_accelerationStructurePtr)
    , m_svdagAndTransform(std::move(other.m_svdagAndTransform))
{
}

template <typename UserState, size_t BlockSize>
inline EvictableResourceID OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::generateCachedBVH(
    DiskDataBatcher& diskDataBatcher,
    gsl::span<const OOCSceneObject*> sceneObjects,
    const std::unordered_map<const OOCGeometricSceneObject*, EvictableResourceHandle<CachedInstanceData, MyCacheT>>& instanceBaseSceneObjectHandleMapping,
    MyCacheT* cache)
{
    flatbuffers::FlatBufferBuilder fbb;
    std::vector<flatbuffers::Offset<serialization::GeometricSceneObjectGeometry>> serializedUniqueGeometry;
    std::vector<flatbuffers::Offset<serialization::InstancedSceneObjectGeometry>> serializedInstancedGeometry;

    std::vector<std::shared_ptr<SceneObjectGeometry>> geometryOwningPointers; // Keep geometry alive until BVH build finished

    // Create leaf nodes for all instanced geometry and then for all non-instanced geometry. It is important to keep these
    // two separate so we can recreate the leafs in the exact same order when deserializing the BVH.
    std::vector<BotLevelLeafNode> leafs;
    std::vector<const OOCGeometricSceneObject*> geometricSceneObjects;
    std::vector<const OOCInstancedSceneObject*> instancedSceneObjects;
    std::vector<EvictableResourceHandle<CachedInstanceData, MyCacheT>> instanceBaseResourceHandles; // ResourceID into the cache to load an instanced base scene object
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* geometricSceneObject = dynamic_cast<const OOCGeometricSceneObject*>(sceneObject)) {
            // Serialize
            std::shared_ptr<SceneObjectGeometry> geometryOwningPointer = geometricSceneObject->getGeometryBlocking();
            const auto* geometry = dynamic_cast<const GeometricSceneObjectGeometry*>(geometryOwningPointer.get());
            serializedUniqueGeometry.push_back(
                geometry->serialize(fbb));
            geometricSceneObjects.push_back(geometricSceneObject);

            // Create bot-level leaf node
            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.push_back(BotLevelLeafNode(geometricSceneObject, geometryOwningPointer, primitiveID));
            }

            geometryOwningPointers.emplace_back(geometryOwningPointer); // Keep geometry alive until BVH build finished
        }
    }
    for (const auto* sceneObject : sceneObjects) {
        if (const auto* instancedSceneObject = dynamic_cast<const OOCInstancedSceneObject*>(sceneObject)) {
            const auto* baseObject = instancedSceneObject->getBaseObject();
            assert(instanceBaseSceneObjectHandleMapping.find(baseObject) != instanceBaseSceneObjectHandleMapping.end());

            // Remember how to construct the base object geometry
            instanceBaseResourceHandles.push_back(instanceBaseSceneObjectHandleMapping.find(baseObject)->second);

            // Store the instancing geometry (which does not store the base object geometry but relies on us to do so)
            auto dummyGeometry = instancedSceneObject->getDummyGeometryBlocking();
            serializedInstancedGeometry.push_back(dummyGeometry.serialize(fbb));

            // Create bot-level node
            leafs.push_back(BotLevelLeafNode(instancedSceneObject, nullptr, nullptr)); // No need to load the underlying geometry & BVH
            instancedSceneObjects.push_back(instancedSceneObject);
        }
    }

    // Build a BVH over all leafs
    WiVeBVH8Build8<BotLevelLeafNode> bvh(leafs);
    auto serializedBVH = bvh.serialize(fbb);

    // Combine the serialized geometric scene objects, instanced scene objects (transform only, no geometry) and bvh
    //  into a single object.
    auto serializedTopLevelLeafNode = serialization::CreateOOCBatchingTopLevelLeafNode(
        fbb,
        fbb.CreateVector(serializedUniqueGeometry),
        fbb.CreateVector(serializedInstancedGeometry),
        serializedBVH,
        static_cast<uint32_t>(leafs.size()));
    fbb.Finish(serializedTopLevelLeafNode);

    // Write the serialized representation to disk
    auto filePart = diskDataBatcher.writeData(fbb);

    auto resourceID = cache->template emplaceFactoryThreadSafe<CachedBlockingPoint>([filePart, geometricSceneObjects = std::move(geometricSceneObjects), instancedSceneObjects = std::move(instancedSceneObjects),
                                                                                        instanceBaseResourceHandles = std::move(instanceBaseResourceHandles)]() -> CachedBlockingPoint {
        g_stats.memory.oocTotalDiskRead += filePart.size;
        auto buffer = filePart.load(); // Needs to stay alive as long as serializedTopLevelLeafNode exists
        const auto* serializedTopLevelLeafNode = serialization::GetOOCBatchingTopLevelLeafNode(buffer.data());

        size_t geometrySize = 0;
        std::vector<std::shared_ptr<SceneObjectGeometry>> geometryOwningPointers;

        // Load geometry/BVH of geometric nodes that are referenced by instancing nodes
        assert(instancedSceneObjects.size() == instanceBaseResourceHandles.size());
        std::vector<std::shared_ptr<CachedInstanceData>> instanceBaseObjects;
        for (const auto resourceHandle : instanceBaseResourceHandles) {
            std::shared_ptr<CachedInstanceData> cachedInstanceBaseData = resourceHandle.getBlocking();
            instanceBaseObjects.push_back(cachedInstanceBaseData);
        }

        // Load unique geometric scene objects
        const auto* serializedUniqueGeometry = serializedTopLevelLeafNode->unique_geometry();
        const auto* serializedInstancedGeometry = serializedTopLevelLeafNode->instanced_geometry();
        std::vector<BotLevelLeafNode> leafs;
        leafs.reserve(serializedTopLevelLeafNode->num_bot_level_leafs());
        for (unsigned i = 0; i < geometricSceneObjects.size(); i++) {
            auto geometry = std::make_shared<GeometricSceneObjectGeometry>(serializedUniqueGeometry->Get(i));

            for (unsigned primitiveID = 0; primitiveID < geometry->numPrimitives(); primitiveID++) {
                leafs.emplace_back(geometricSceneObjects[i], geometry, primitiveID);
            }

            geometrySize += geometry->sizeBytes();
            geometryOwningPointers.emplace_back(std::move(geometry));
        }

        // Load instanced scene objects
        assert(instanceBaseObjects.size() == serializedInstancedGeometry->Length());
        for (size_t i = 0; i < instanceBaseObjects.size(); i++) {
            const auto& [baseGeometry, baseBVH] = *instanceBaseObjects[i];

            // Make sure to count the instance geometry class itself towards the memory limit. Although the base objects (the objects
            // being pointed to) are already accounted for (they are stored in the cache separately), we need to make sure that we do
            // count the instance objects themselves (because of the transform class, they are actually quite big).
            geometrySize += sizeof(InstancedSceneObjectGeometry);

            auto geometry = std::make_shared<InstancedSceneObjectGeometry>(
                serializedInstancedGeometry->Get(static_cast<flatbuffers::uoffset_t>(i)),
                baseGeometry);
            leafs.push_back(BotLevelLeafNode(instancedSceneObjects[i], geometry, baseBVH));

            geometryOwningPointers.emplace_back(std::move(geometry));
        }

        auto bvh = WiVeBVH8Build8<BotLevelLeafNode>(serializedTopLevelLeafNode->bvh(), std::move(leafs));
        return CachedBlockingPoint {
            geometrySize,
            std::move(bvh),
            std::move(geometryOwningPointers)
        };
    });
    return resourceID;
}

template <typename UserState, size_t BlockSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::computeBounds(
    gsl::span<const OOCSceneObject*> sceneObjects)
{
    Bounds ret;
    for (const auto* sceneObject : sceneObjects) {
        ret.extend(sceneObject->worldBounds());
    }
    return ret;
}

template <typename UserState, size_t BlockSize>
inline std::pair<SparseVoxelDAG, typename OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::SVDAGRayOffset> OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::computeSVDAG(gsl::span<const OOCSceneObject*> sceneObjects)
{
    Bounds gridBounds;
    for (const auto* sceneObject : sceneObjects) {
        gridBounds.extend(sceneObject->worldBounds());
    }

    VoxelGrid voxelGrid(OUT_OF_CORE_SVDAG_RESOLUTION);
    for (const auto* sceneObject : sceneObjects) {
        auto geometry = sceneObject->getGeometryBlocking();
        geometry->voxelize(voxelGrid, gridBounds);
    }

    // SVO is at (1, 1, 1) to (2, 2, 2)
    float maxDim = maxComponent(gridBounds.extent());

    // SVDAG compression occurs after all top level leaf nodes have been created
    SparseVoxelDAG svdag(voxelGrid);
    return { std::move(svdag), SVDAGRayOffset { gridBounds.min, glm::vec3(1.0f / maxDim) } };
}

template <typename UserState, size_t BlockSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::getBounds() const
{
    return m_bounds;
}

template <typename UserState, size_t BlockSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::intersect(
    Ray& ray,
    RayHit& hitInfo,
    const UserState& userState,
    PauseableBVHInsertHandle insertHandle) const
{
    if constexpr (OUT_OF_CORE_OCCLUSION_CULLING) {
        //auto scopedTimings = g_stats.timings.svdagTraversalTime.getScopedStopwatch();
        if constexpr (ENABLE_ADDITIONAL_STATISTICS) {
            auto scopedTimings = g_stats.timings.svdagTraversalTime.getScopedStopwatch();
            g_stats.svdag.numIntersectionTests += 1;

            const auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
            auto svdagRay = ray;
            svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
            auto tOpt = svdag.intersectScalar(svdagRay);
            if (!tOpt) {
                g_stats.svdag.numRaysCulled++;
                return false; // Missed, continue traversal
            }
        } else {
            const auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
            auto svdagRay = ray;
            svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
            auto tOpt = svdag.intersectScalar(svdagRay);
            if (!tOpt) {
                return false; // Missed, continue traversal
            }
        }
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    RayBlock* block = mutThisPtr->m_threadLocalActiveBlock.local();
    if (!block || block->full()) {
        if (block) {
            // Block was full, move it to the list of immutable blocks
            auto* oldHead = mutThisPtr->m_immutableRayBlockList.load();
            do {
                block->setNext(oldHead);
            } while (!mutThisPtr->m_immutableRayBlockList.compare_exchange_weak(oldHead, block));

            mutThisPtr->m_numFullBlocks.fetch_add(1);
        }

        // Allocate a new block and set it as the new active block
        auto* mem = mutThisPtr->m_accelerationStructurePtr->m_blockAllocator.allocate();
        block = new (mem) RayBlock();
        mutThisPtr->m_threadLocalActiveBlock.local() = block;
    }

    bool success = block->tryPush(ray, hitInfo, userState, insertHandle);
    assert(success);

    return {}; // Paused
}

template <typename UserState, size_t BlockSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    if constexpr (OUT_OF_CORE_OCCLUSION_CULLING) {
        if constexpr (ENABLE_ADDITIONAL_STATISTICS) {
            auto scopedTimings = g_stats.timings.svdagTraversalTime.getScopedStopwatch();
            g_stats.svdag.numIntersectionTests++;

            auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
            auto svdagRay = ray;
            svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
            auto tOpt = svdag.intersectScalar(svdagRay);
            if (!tOpt) {
                g_stats.svdag.numRaysCulled++;
                return false; // Missed, continue traversal
            }
        } else {
            auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
            auto svdagRay = ray;
            svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
            auto tOpt = svdag.intersectScalar(svdagRay);
            if (!tOpt) {
                return false; // Missed, continue traversal
            }
        }
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    RayBlock* block = mutThisPtr->m_threadLocalActiveBlock.local();
    if (!block || block->full()) {
        if (block) {
            // Block was full, move it to the list of immutable blocks
            auto* oldHead = mutThisPtr->m_immutableRayBlockList.load();
            do {
                block->setNext(oldHead);
            } while (!mutThisPtr->m_immutableRayBlockList.compare_exchange_weak(oldHead, block));

            mutThisPtr->m_numFullBlocks.fetch_add(1);
        }

        // Allocate a new block and set it as the new active block
        auto* mem = mutThisPtr->m_accelerationStructurePtr->m_blockAllocator.allocate();
        block = new (mem) RayBlock();
        mutThisPtr->m_threadLocalActiveBlock.local() = block;
    }

    bool success = block->tryPush(ray, userState, insertHandle);
    assert(success);

    return {}; // Paused
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::forwardPartiallyFilledBlocks()
{
    bool forwardedBlocks = false;

    OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock* outBlock = m_immutableRayBlockList;
    int forwardedRays = 0;
    for (auto& block : m_threadLocalActiveBlock) {
        if (block) {
            for (const auto& [ray, hitInfoOpt, userState, insertHandle] : *block) {
                if (!outBlock || outBlock->full()) {
                    auto* mem = m_accelerationStructurePtr->m_blockAllocator.allocate();
                    auto* newBlock = new (mem) RayBlock();

                    newBlock->setNext(outBlock);
                    outBlock = newBlock;
                    m_numFullBlocks.fetch_add(1);
                }

                if (hitInfoOpt) {
                    outBlock->tryPush(ray, *hitInfoOpt, userState, insertHandle);
                } else {
                    outBlock->tryPush(ray, userState, insertHandle);
                }
                forwardedRays++;
            }

            m_accelerationStructurePtr->m_blockAllocator.deallocate(block);
            forwardedBlocks = true;
        }
        block = nullptr; // Reset thread-local block
    }
    m_immutableRayBlockList = outBlock;

    return forwardedBlocks;
}

template <typename UserState, size_t BlockSize>
inline void OOCBatchingAccelerationStructure<UserState, BlockSize>::flush()
{
    int i = 0;
    while (true) {
        std::cout << "FLUSHING " << (i++) << std::endl;

        bool done = true;
        for (auto* node : m_bvh.leafs()) {
            done = done && !node->hasBatchedRays();

            // Put this in a separate variable because doing the following will only execute when done is false:
            // done = done && !forwardedBlocks()
            bool forwardedBlocks = node->forwardPartiallyFilledBlocks();
            done = done && !forwardedBlocks;
        }

        if (done)
            break;

        TopLevelLeafNode::flushRange(m_bvh.leafs(), this);

    }

    std::cout << "FLUSH COMPLETE" << std::endl;
}

template <typename UserState, size_t BlockSize>
void OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::flushRange(
    gsl::span<TopLevelLeafNode*> nodes,
    OOCBatchingAccelerationStructure<UserState, BlockSize>* accelerationStructurePtr)
{
    const size_t hwConcurrency = std::thread::hardware_concurrency();
    const size_t cacheConcurrency = accelerationStructurePtr->m_numLoadingThreads;
    const size_t flowConcurrency = 2 * cacheConcurrency;

    std::vector<TopLevelLeafNode*> nodesWithBatchedRays;
    std::copy_if(std::begin(nodes), std::end(nodes), std::back_inserter(nodesWithBatchedRays), [](TopLevelLeafNode* n) {
        return n->hasBatchedRays();
    });

    std::cout << "Number of nodes with batched rays: " << nodesWithBatchedRays.size() << "\n";

    std::sort(std::begin(nodesWithBatchedRays), std::end(nodesWithBatchedRays), [](const auto* node1, const auto* node2) {
        return node1->m_numFullBlocks.load() > node2->m_numFullBlocks.load(); // Sort from big to small
    });

    // Only flush nodes that have a lot of flushed blocks, wait for other nodes for their blocks to fill up.
    const int blocksThreshold = nodesWithBatchedRays.empty() ? 0 : nodesWithBatchedRays[0]->m_numFullBlocks.load() / 5;

    std::mutex flushInfoMutex;
    RenderStats::FlushInfo& flushInfo = g_stats.flushInfos.emplace_back();
    flushInfo.numBatchingPointsWithRays = static_cast<int>(nodesWithBatchedRays.size());

    std::atomic_size_t currentNodeIndex = 0; // source_node is always run sequentially
    using BlockWithoutGeom = std::pair<RayBlock*, EvictableResourceID>;
    tbb::flow::graph g;
    tbb::flow::source_node<BlockWithoutGeom> sourceNode(
        g,
        [&](BlockWithoutGeom& out) -> bool {
            auto nodeIndex = currentNodeIndex.fetch_add(1);
            if (nodeIndex >= nodesWithBatchedRays.size()) {
                // All nodes processed
                return false;
            }

            auto* node = nodesWithBatchedRays[nodeIndex];
            if (node->m_numFullBlocks.load() < blocksThreshold) {
                // Don't process nodes with very little rays batched (probably not it to load nodes from disk)
                return false;
            }

            {
                std::scoped_lock l(flushInfoMutex);
                flushInfo.approximateRaysPerFlushedBatchingPoint.push_back(node->m_numFullBlocks.load() * BlockSize);
            }

            RayBlock* block = node->m_immutableRayBlockList.exchange(nullptr);
            node->m_numFullBlocks.store(0);
            out = { block, node->m_geometryDataCacheID };
            return true;
        });

    // Prevent TBB from loading all items from the cache before starting traversal
    tbb::flow::limiter_node<BlockWithoutGeom> flowLimiterNode(g, flowConcurrency);

    // For each of those leaf nodes, load the geometry (asynchronously)
    auto cacheSubGraph = std::move(accelerationStructurePtr->m_geometryCache.template getFlowGraphNode<RayBlock*, CachedBlockingPoint>(g));

    // Create a task for each block associated with that leaf node (for increased parallelism)
    using BlockWithGeom = std::pair<RayBlock*, std::shared_ptr<CachedBlockingPoint>>;
    using BlockWithGeomLimited = std::pair<std::pair<std::shared_ptr<std::atomic_int>, RayBlock*>, std::shared_ptr<CachedBlockingPoint>>;
    using BlockNodeType = tbb::flow::multifunction_node<BlockWithGeom, tbb::flow::tuple<BlockWithGeomLimited>>;
    BlockNodeType blockNode(
        g,
        tbb::flow::unlimited,
        [&](const BlockWithGeom& v, typename BlockNodeType::output_ports_type& op) {
            auto [firstBlock, geometry] = v;

            // Count the number of blocks
            int numBlocks = 0;
            const auto* tmpBlock = firstBlock;
            while (tmpBlock) {
                numBlocks++;
                tmpBlock = tmpBlock->next();
            }
            if (numBlocks == 0) {
                return;
            }

            // Flow limitter
            auto unprocessedBlocksCounter = std::make_shared<std::atomic_int>(numBlocks);
            size_t raysProcessed = 0;
            auto* block = firstBlock;
            while (block) {
                bool success = std::get<0>(op).try_put({ { unprocessedBlocksCounter, block }, geometry });
                assert(success);
                block = block->next();
            }
        });

    using TraverseNodeType = tbb::flow::multifunction_node<BlockWithGeomLimited, tbb::flow::tuple<tbb::flow::continue_msg>>;
    TraverseNodeType traversalNode(
        g,
        tbb::flow::unlimited,
        [&](const BlockWithGeomLimited& v, typename TraverseNodeType::output_ports_type& op) {
            auto [blockInfo, geometryData] = v;
            auto [unprocessedBlockCounter, block] = blockInfo;

            for (auto [ray, rayHitOpt, userState, insertHandle] : *block) {
                // Intersect with the bottom-level BVH
                if (rayHitOpt) {
                    RayHit& rayHit = *rayHitOpt;
                    geometryData->leafBVH.intersect(gsl::make_span(&ray, 1), gsl::make_span(&rayHit, 1));
                } else {
                    geometryData->leafBVH.intersectAny(gsl::make_span(&ray, 1));
                }
            }

            for (auto [ray, rayHitOpt, userState, insertHandle] : *block) {
                if (rayHitOpt) {
                    // Insert the ray back into the top-level  BVH
                    auto optResult = accelerationStructurePtr->m_bvh.intersect(ray, *rayHitOpt, userState, insertHandle);
                    if (optResult && *optResult == false) {
                        // Ray exited the system so we can run the hit/miss shaders
                        if (rayHitOpt->primitiveID != -1) {
                            const auto& hitSceneObjectInfo = std::get<RayHit::OutOfCore>(rayHitOpt->sceneObjectVariant);
                            auto si = hitSceneObjectInfo.sceneObjectGeometry->fillSurfaceInteraction(ray, *rayHitOpt);
                            si.sceneObject = hitSceneObjectInfo.sceneObject;
                            auto material = hitSceneObjectInfo.sceneObject->getMaterialBlocking();
                            si.sceneObjectMaterial = material.get();
                            accelerationStructurePtr->m_hitCallback(ray, si, userState);
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

            if (unprocessedBlockCounter->fetch_sub(1) == 1) {
                auto success = std::get<0>(op).try_put(tbb::flow::continue_msg());
                assert(success);
            }

            accelerationStructurePtr->m_blockAllocator.deallocate(block);
        });

    // NOTE: The source starts outputting as soon as an edge is connected.
    //       So make sure it is the last edge that we connect.
    cacheSubGraph.connectInput(flowLimiterNode);
    cacheSubGraph.connectOutput(blockNode);
    tbb::flow::make_edge(blockNode, traversalNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(traversalNode), flowLimiterNode.decrement);
    tbb::flow::make_edge(sourceNode, flowLimiterNode);
    g.wait_for_all();
}

template <typename UserState, size_t BlockSize>
inline void OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes)
{
    std::vector<SparseVoxelDAG*> dags;
    for (auto* node : nodes) {
        dags.push_back(&std::get<0>(node->m_svdagAndTransform));
    }
    SparseVoxelDAG::compressDAGs(dags);

    for (const auto* dag : dags) {
        g_stats.memory.svdags += dag->sizeBytes();
    }
}

template <typename UserState, size_t BlockSize>
inline size_t OOCBatchingAccelerationStructure<UserState, BlockSize>::TopLevelLeafNode::sizeBytes() const
{
    size_t size = sizeof(decltype(*this));
    size += m_threadLocalActiveBlock.size() * sizeof(RayBlock*);
    size += std::get<0>(m_svdagAndTransform).sizeBytes();
    return size;
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNodeInstanced::BotLevelLeafNodeInstanced(
    const SceneObjectGeometry* baseSceneObjectGeometry,
    unsigned primitiveID)
    : m_baseSceneObjectGeometry(baseSceneObjectGeometry)
    , m_primitiveID(primitiveID)
{
}

template <typename UserState, size_t BlockSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNodeInstanced::getBounds() const
{
    return m_baseSceneObjectGeometry->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNodeInstanced::intersect(Ray& ray, RayHit& hitInfo) const
{
    return m_baseSceneObjectGeometry->intersectPrimitive(ray, hitInfo, m_primitiveID);
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCGeometricSceneObject* sceneObject,
    const std::shared_ptr<SceneObjectGeometry>& sceneObjectGeometry,
    unsigned primitiveID)
    : m_data(Regular { sceneObject, sceneObjectGeometry, primitiveID })
{
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNode::BotLevelLeafNode(
    const OOCInstancedSceneObject* sceneObject,
    const std::shared_ptr<SceneObjectGeometry>& sceneObjectGeometry,
    const std::shared_ptr<WiVeBVH8Build8<OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNodeInstanced>>& bvh)
    : m_data(Instance { sceneObject, sceneObjectGeometry, bvh })
{
}

template <typename UserState, size_t BlockSize>
inline Bounds OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNode::getBounds() const
{
    if (std::holds_alternative<Regular>(m_data)) {
        const auto& data = std::get<Regular>(m_data);
        return data.sceneObjectGeometry->worldBoundsPrimitive(data.primitiveID);
    } else {
        return std::get<Instance>(m_data).sceneObject->worldBounds();
    }
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::BotLevelLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
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

        RayHit localHitInfo;
        Ray localRay = data.sceneObject->transformRayToInstanceSpace(ray);
        data.bvh->intersect(gsl::make_span(&localRay, 1), gsl::make_span(&localHitInfo, 1));
        if (localHitInfo.primitiveID != -1) {
            ray.tfar = localRay.tfar;
            hitInfo = localHitInfo;
            hitInfo.sceneObjectVariant = RayHit::OutOfCore { data.sceneObject, data.sceneObjectGeometry };
            return true;
        } else {
            return false;
        }
    }
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::tryPush(const Ray& ray, const RayHit& rayHit, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, rayHit, state, insertHandle);
    return true;
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::tryPush(const Ray& ray, const UserState& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, state, insertHandle);
    return true;
}

template <typename UserState, size_t BlockSize>
inline const typename OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::begin()
{
    return iterator(this, 0);
}

template <typename UserState, size_t BlockSize>
inline const typename OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::end()
{
    return iterator(this, m_data.size());
}

template <typename UserState, size_t BlockSize>
inline OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::iterator(OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock* block, size_t index)
    : m_rayBlock(block)
    , m_index(index)
{
}

template <typename UserState, size_t BlockSize>
inline typename OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator& OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::operator++()
{
    m_index++;
    return *this;
}

template <typename UserState, size_t BlockSize>
inline typename OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::operator++(int)
{
    auto r = *this;
    m_index++;
    return r;
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::operator==(OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator other) const
{
    assert(m_rayBlock == other.m_rayBlock);
    return m_index == other.m_index;
}

template <typename UserState, size_t BlockSize>
inline bool OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::operator!=(OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator other) const
{
    assert(m_rayBlock == other.m_rayBlock);
    return m_index != other.m_index;
}

template <typename UserState, size_t BlockSize>
inline typename OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::value_type OOCBatchingAccelerationStructure<UserState, BlockSize>::RayBlock::iterator::operator*()
{
    auto& [ray, rayHitOpt, userState, insertHandle] = m_rayBlock->m_data[m_index];
    return { ray, rayHitOpt, userState, insertHandle };
}
}
