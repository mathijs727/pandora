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
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
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
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_sort.h>
#include <tbb/reader_writer_lock.h>
#include <tbb/task_group.h>

using namespace std::string_literals;

namespace pandora {

static constexpr unsigned OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF = 25000;
static constexpr bool OUT_OF_CORE_OCCLUSION_CULLING = false;

template <typename UserState, size_t BatchSize = 32>
class OOCBatchingAccelerationStructure {
public:
    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&, const InsertHandle&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    OOCBatchingAccelerationStructure(
        size_t geometryCacheSize,
        std::filesystem::path scratchFolder,
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
        const RayBatch* next() const { return m_nextPtr; }
        bool full() const { return m_data.full(); }

        bool tryPush(const Ray& ray, const SurfaceInteraction& si, const UserState& state, const PauseableBVHInsertHandle& insertHandle);
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
            std::filesystem::path cacheFile,
            gsl::span<const OOCSceneObject*> sceneObjects,
            FifoCache<GeometryData>* geometryCache,
            OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);
        TopLevelLeafNode(TopLevelLeafNode&& other);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, SurfaceInteraction& si, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        bool forwardPartiallyFilledBatches(); // Adds the active batches to the list of immutable batches (even if they are not full)
        // Flush a whole range of nodes at a time as opposed to a non-static flush member function which would require a
        // separate tbb flow graph for each node that is processed.
        static void flushRange(
            gsl::span<TopLevelLeafNode*> nodes,
            OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

        static void compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes);

        size_t sizeBytes() const;

    private:
        static EvictableResourceID generateCachedBVH(
            std::filesystem::path cacheFile,
            gsl::span<const OOCSceneObject*> sceneObjects,
            FifoCache<GeometryData>* cache);

        struct SVDAGRayOffset {
            glm::vec3 gridBoundsMin;
            glm::vec3 invGridBoundsExtent;
        };
        static std::pair<SparseVoxelDAG, SVDAGRayOffset> computeSVDAG(gsl::span<const OOCSceneObject*> sceneObjects);

    private:
        EvictableResourceID m_geometryDataCacheID;
        std::vector<const OOCSceneObject*> m_sceneObjects;

        std::atomic_int m_numFullBatches;
        tbb::enumerable_thread_specific<RayBatch*> m_threadLocalActiveBatch;
        std::atomic<RayBatch*> m_immutableRayBatchList;
        OOCBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;

        std::pair<SparseVoxelDAG, SVDAGRayOffset> m_svdagAndTransform;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(
        std::filesystem::path scratchFolder,
        FifoCache<typename TopLevelLeafNode::GeometryData>* cache,
        gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects,
        OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

private:
    FifoCache<typename TopLevelLeafNode::GeometryData> m_geometryCache;

    tbb::reader_writer_lock m_nodesListMutex;
    std::vector<TopLevelLeafNode*> m_nodesWithFullBatches;

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
    std::filesystem::path scratchFolder,
    const Scene& scene,
    HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_geometryCache(
          geometryCacheSize,
          [](size_t bytes) { g_stats.memory.botLevelLoaded += bytes; },
          [](size_t bytes) { g_stats.memory.botLevelEvicted += bytes; })
    , m_batchAllocator()
    , m_bvh(std::move(buildBVH(scratchFolder, &m_geometryCache, scene.getOOCSceneObjects(), this)))
    , m_threadLocalPreallocatedRaybatch([&]() { return m_batchAllocator.allocate(); })
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
    // Clean the scenes geometry cache because it won't be used anymore. The batches recreate the geometry
    // and use their own cache to manage it.
    scene.geometryCache()->evictAllUnsafe();

    g_stats.memory.topBVH += m_bvh.sizeBytes();
    for (const auto* leaf : m_bvh.leafs()) {
        g_stats.memory.topBVHLeafs += leaf->sizeBytes();
    }
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
        TopLevelLeafNode::flushRange(m_bvh.leafs(), this);

        bool forwardedBatch = false;
        for (auto* node : m_bvh.leafs()) {
            forwardedBatch = forwardedBatch || node->forwardPartiallyFilledBatches();
        }

        if (!forwardedBatch)
            break;
    }

    std::cout << "FLUSH COMPLETE" << std::endl;
}

template <typename UserState, size_t BatchSize>
inline PauseableBVH4<typename OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> OOCBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    std::filesystem::path scratchFolder,
    FifoCache<typename TopLevelLeafNode::GeometryData>* cache,
    gsl::span<const std::unique_ptr<OOCSceneObject>> sceneObjects,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr)
{
    if (!std::filesystem::exists(scratchFolder)) {
        std::filesystem::create_directories(scratchFolder);
    }
    ALWAYS_ASSERT(std::filesystem::is_directory(scratchFolder));

    auto sceneObjectGroups = groupSceneObjects(OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF, sceneObjects);

    std::mutex m;
    std::vector<TopLevelLeafNode> leafs;
    tbb::parallel_for(tbb::blocked_range<size_t>(0llu, sceneObjectGroups.size()), [&](tbb::blocked_range<size_t> localRange) {
        for (size_t i = localRange.begin(); i < localRange.end(); i++) {
            std::filesystem::path cacheFile = scratchFolder / ("node"s + std::to_string(i) + ".bin"s);
            TopLevelLeafNode leaf(cacheFile, sceneObjectGroups[i], cache, accelerationStructurePtr);
            {
                std::scoped_lock<std::mutex> l(m);
                leafs.push_back(std::move(leaf));
            }
        }
    });

    auto ret = PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
    TopLevelLeafNode::compressSVDAGs(ret.leafs());
    return std::move(ret);
}

template <typename UserState, size_t BatchSize>
inline OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    std::filesystem::path cacheFile,
    gsl::span<const OOCSceneObject*> sceneObjects,
    FifoCache<GeometryData>* geometryCache,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructure)
    : m_geometryDataCacheID(generateCachedBVH(cacheFile, sceneObjects, geometryCache))
    , m_threadLocalActiveBatch([]() { return nullptr; })
    , m_immutableRayBatchList(nullptr)
    , m_accelerationStructurePtr(accelerationStructure)
    , m_svdagAndTransform(computeSVDAG(sceneObjects))
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
    , m_svdagAndTransform(std::move(other.m_svdagAndTransform))
{
}

template <typename UserState, size_t BatchSize>
inline EvictableResourceID OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::generateCachedBVH(
    std::filesystem::path cacheFilePath,
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
        serializedBVH,
        static_cast<uint32_t>(leafs.size()));
    fbb.Finish(serializedTopLevelLeafNode);

    std::ofstream file;
    file.open(cacheFilePath, std::ios::out | std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    file.close();

    auto resourceID = cache->emplaceFactoryThreadSafe([cacheFilePath, geometricSceneObjects = std::move(geometricSceneObjects), instancedSceneObjects = std::move(instancedSceneObjects)]() -> GeometryData {
        auto mmapFile = mio::mmap_source(cacheFilePath.string(), 0, mio::map_entire_file);
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
            leafs.reserve(geometry->numPrimitives());
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
        leafs.reserve(serializedTopLevelLeafNode->num_bot_level_leafs());
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
inline std::pair<SparseVoxelDAG, typename OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::SVDAGRayOffset> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::computeSVDAG(gsl::span<const OOCSceneObject*> sceneObjects)
{
    Bounds gridBounds;
    for (const auto* sceneObject : sceneObjects) {
        gridBounds.extend(sceneObject->worldBounds());
    }

    VoxelGrid voxelGrid(64);
    for (const auto* sceneObject : sceneObjects) {
        auto geometry = sceneObject->getGeometryBlocking();
        geometry->voxelize(voxelGrid, gridBounds);
    }

    // SVO is at (1, 1, 1) to (2, 2, 2)
    float maxDim = maxComponent(gridBounds.extent());

    SparseVoxelDAG svdag(voxelGrid);
    // NOTE: the svdags are already being compressed together. Compressing here too will cost more compute power but will reduce memory usage.
    /*{
        std::vector svdags = { &svdag };
        compressDAGs(svdags);
    }*/
    return { std::move(svdag), SVDAGRayOffset { gridBounds.min, glm::vec3(1.0f / maxDim) } };
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
    if constexpr (OUT_OF_CORE_OCCLUSION_CULLING) {
        auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
        auto svdagRay = ray;
        svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
        if (!svdag.intersectScalar(svdagRay))
            return false; // Missed, continue traversal
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    RayBatch* batch = mutThisPtr->m_threadLocalActiveBatch.local();
    if (!batch || batch->full()) {
        if (batch) {
            // Batch was full, move it to the list of immutable batches
            auto* oldHead = mutThisPtr->m_immutableRayBatchList.load();
            do {
                batch->setNext(oldHead);
            } while (!mutThisPtr->m_immutableRayBatchList.compare_exchange_weak(oldHead, batch));

            if (!oldHead) {
                tbb::reader_writer_lock::scoped_lock l(mutThisPtr->m_accelerationStructurePtr->m_nodesListMutex);
                mutThisPtr->m_accelerationStructurePtr->m_nodesWithFullBatches.push_back(mutThisPtr);
            }
            mutThisPtr->m_numFullBatches.fetch_add(1);
        }

        // Allocate a new batch and set it as the new active batch
        batch = mutThisPtr->m_accelerationStructurePtr->m_batchAllocator.allocate();
        mutThisPtr->m_threadLocalActiveBatch.local() = batch;
        g_stats.memory.batches += batch->sizeBytes();
    }

    bool success = batch->tryPush(ray, si, userState, insertHandle);
    assert(success);

    return {}; // Paused
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    if constexpr (OUT_OF_CORE_OCCLUSION_CULLING) {
        auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
        auto svdagRay = ray;
        svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
        if (!svdag.intersectScalar(svdagRay))
            return false; // Missed, continue traversal
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    RayBatch* batch = mutThisPtr->m_threadLocalActiveBatch.local();
    if (!batch || batch->full()) {
        if (batch) {
            // Batch was full, move it to the list of immutable batches
            auto* oldHead = mutThisPtr->m_immutableRayBatchList.load();
            do {
                batch->setNext(oldHead);
            } while (!mutThisPtr->m_immutableRayBatchList.compare_exchange_weak(oldHead, batch));

            if (!oldHead) {
                tbb::reader_writer_lock::scoped_lock l(mutThisPtr->m_accelerationStructurePtr->m_nodesListMutex);
                mutThisPtr->m_accelerationStructurePtr->m_nodesWithFullBatches.push_back(mutThisPtr);
            }
            mutThisPtr->m_numFullBatches.fetch_add(1);
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
inline bool OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::forwardPartiallyFilledBatches()
{
    bool forwardedBatch = false;

    for (auto& batch : m_threadLocalActiveBatch) {
        if (batch) {
            batch->setNext(m_immutableRayBatchList);
            m_immutableRayBatchList = batch;
            forwardedBatch = true;
        }
        batch = nullptr;
    }

    if (forwardedBatch) {
        tbb::reader_writer_lock::scoped_lock l(m_accelerationStructurePtr->m_nodesListMutex);
        m_accelerationStructurePtr->m_nodesWithFullBatches.push_back(this);
    }

    return forwardedBatch;
}

template <typename UserState, size_t BatchSize>
void OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flushRange(
    gsl::span<TopLevelLeafNode*> nodes,
    OOCBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr)
{
    tbb::flow::graph g;

    // Generate a task for each top-level leaf node with at least one non-empty batch
    using BatchWithoutGeom = std::pair<RayBatch*, EvictableResourceID>;
    tbb::flow::source_node<BatchWithoutGeom> sourceNode(
        g,
        [&](BatchWithoutGeom& out) -> bool {
            auto& nodesWithFullBatches = accelerationStructurePtr->m_nodesWithFullBatches;

            size_t nodeIndex = -1;
            TopLevelLeafNode* node = nullptr;
            {
                tbb::reader_writer_lock::scoped_lock_read l(accelerationStructurePtr->m_nodesListMutex);
                auto nodeIter = std::max_element(
                    std::begin(nodesWithFullBatches),
                    std::end(nodesWithFullBatches),
                    [](const auto* node1, const auto* node2) {
                        // These values might change while new batches are added, but thats not important here because
                        // its just a heuristic
                        return node1->m_numFullBatches.load() < node2->m_numFullBatches.load();
                    });

                if (nodeIter != std::end(nodesWithFullBatches)) {
                    nodeIndex = nodeIter - std::begin(nodesWithFullBatches); // Iterators are invalidated when vector changes, index is not
                    node = *nodeIter;
                }
            }

            if (!node)
                return false;

            // Get a write lock and remove the node from the list
            {
                tbb::reader_writer_lock::scoped_lock l(accelerationStructurePtr->m_nodesListMutex);
                std::swap(std::begin(nodesWithFullBatches) + nodeIndex, std::end(nodesWithFullBatches) - 1);
                nodesWithFullBatches.pop_back();
            }
            RayBatch* batch = node->m_immutableRayBatchList.exchange(nullptr);
            node->m_numFullBatches.store(0);

            out = { batch, node->m_geometryDataCacheID };
            return true;
        });

    // Prevent TBB from loading all items from the cache before starting traversal
    tbb::flow::limiter_node<BatchWithoutGeom> flowLimiterNode(g, std::thread::hardware_concurrency());

    // For each of those leaf nodes, load the geometry (asynchronously)
    auto cacheSubGraph = std::move(accelerationStructurePtr->m_geometryCache.template getFlowGraphNode<RayBatch*>(g));

    // Create a task for each batch associated with that leaf node (for increased parallelism)
    using BatchWithGeom = std::pair<RayBatch*, std::shared_ptr<GeometryData>>;
    using BatchWithGeomLimited = std::pair<std::pair<std::shared_ptr<std::atomic_int>, RayBatch*>, std::shared_ptr<GeometryData>>;
    using BatchNodeType = tbb::flow::multifunction_node<BatchWithGeom, tbb::flow::tuple<BatchWithGeomLimited>>;
    BatchNodeType batchNode(
        g,
        tbb::flow::unlimited,
        [&](const BatchWithGeom& v, typename BatchNodeType::output_ports_type& op) {
            auto [firstBatch, geometry] = v;

            int numBatches = 0;

            const auto* tmpBatch = firstBatch;
            while (tmpBatch) {
                numBatches++;
                tmpBatch = tmpBatch->next();
            }
            if (numBatches == 0) {
                return;
            }

            auto unprocessedBatchesCount = std::make_shared<std::atomic_int>(numBatches);
            size_t raysProcessed = 0;
            auto* batch = firstBatch;
            while (batch) {
                ALWAYS_ASSERT(std::get<0>(op).try_put({ { unprocessedBatchesCount, batch }, geometry }));
                batch = batch->next();
            }
        });

    using TraverseNodeType = tbb::flow::multifunction_node<BatchWithGeomLimited, tbb::flow::tuple<tbb::flow::continue_msg>>;
    TraverseNodeType traversalNode(
        g,
        tbb::flow::unlimited,
        [&](const BatchWithGeomLimited& v, typename TraverseNodeType::output_ports_type& op) {
            auto [batchInfo, geometryData] = v;
            auto [unprocessedBatchCounter, batch] = batchInfo;

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

            if (unprocessedBatchCounter->fetch_sub(1) == 1) {
                ALWAYS_ASSERT(std::get<0>(op).try_put(tbb::flow::continue_msg()));
            }

            accelerationStructurePtr->m_batchAllocator.deallocate(batch);
        });

    // NOTE: The source starts outputting as soon as an edge is connected.
    //       So make sure it is the last edge that we connect.
    cacheSubGraph.connectInput(flowLimiterNode);
    cacheSubGraph.connectOutput(batchNode);
    tbb::flow::make_edge(batchNode, traversalNode);
    tbb::flow::make_edge(tbb::flow::output_port<0>(traversalNode), flowLimiterNode.decrement);
    tbb::flow::make_edge(sourceNode, flowLimiterNode);
    g.wait_for_all();
}

template <typename UserState, size_t BatchSize>
inline void OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes)
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

template <typename UserState, size_t BatchSize>
inline size_t OOCBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::sizeBytes() const
{
    size_t size = sizeof(decltype(*this));
    size += m_sceneObjects.capacity() * sizeof(const OOCSceneObject*);
    size += m_threadLocalActiveBatch.size() * sizeof(RayBatch*);
    size += std::get<0>(m_svdagAndTransform).sizeBytes();
    return size;
}

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
