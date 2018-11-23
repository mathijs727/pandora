#pragma once
#include "pandora/core/interaction.h"
#include "pandora/core/ray.h"
#include "pandora/core/scene.h"
#include "pandora/core/stats.h"
#include "pandora/geometry/group.h"
#include "pandora/scene/geometric_scene_object.h"
#include "pandora/scene/instanced_scene_object.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/traversal/bvh/wive_bvh8_build8.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/growing_free_list_ts.h"
#include <gsl/gsl>
#include <memory>
#include <numeric>
#include <optional>
#include <string>

using namespace std::string_literals;

namespace pandora {

static constexpr unsigned IN_CORE_BATCHING_SCENE_OBJECT_PRIMS = 512;
static constexpr unsigned IN_CORE_BATCHING_PRIMS_PER_LEAF = 4096;
static constexpr bool IN_CORE_OCCLUSION_CULLING = false;
static constexpr size_t IN_CORE_SVDAG_RESOLUTION = 32;

template <typename UserState, size_t BatchSize = 384>
class InCoreBatchingAccelerationStructure {
public:
    constexpr static bool recurseTillCompletion = false; // Rays are batched, so paths are not traced untill completion

    using InsertHandle = void*;
    using HitCallback = std::function<void(const Ray&, const SurfaceInteraction&, const UserState&)>;
    using AnyHitCallback = std::function<void(const Ray&, const UserState&)>;
    using MissCallback = std::function<void(const Ray&, const UserState&)>;

public:
    InCoreBatchingAccelerationStructure(
        const Scene& scene,
        HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback);
    ~InCoreBatchingAccelerationStructure() = default;

    // Dummy
    bool placeIntersectRequestReturnOnMiss(const Ray& ray, const UserState& userState) { return false; }

    void placeIntersectRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);
    void placeIntersectAnyRequests(gsl::span<const Ray> rays, gsl::span<const UserState> perRayUserData, const InsertHandle& insertHandle = nullptr);

    void flush();

private:
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

        bool tryPush(const Ray& ray, const RayHit& rayHit, UserState&& state, const PauseableBVHInsertHandle& insertHandle);
        bool tryPush(const Ray& ray, UserState&& state, const PauseableBVHInsertHandle& insertHandle);

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
            BatchItem(const Ray& ray, UserState&& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , rayHit({})
                , userState(std::move(userState))
                , insertHandle(insertHandle)
            {
            }
            BatchItem(const Ray& ray, const RayHit& rayHit, UserState&& userState, const PauseableBVHInsertHandle& insertHandle)
                : ray(ray)
                , rayHit(rayHit)
                , userState(std::move(userState))
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
        BotLevelLeafNodeInstanced(const InCoreGeometricSceneObject* baseSceneObject, unsigned primitiveID);

        Bounds getBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        const InCoreGeometricSceneObject* m_baseSceneObject;
        const unsigned m_primitiveID;
    };

    class BotLevelLeafNode {
    public:
        BotLevelLeafNode(const InCoreGeometricSceneObject* sceneObject, unsigned primitiveID);
        //BotLevelLeafNode(const InCoreInstancedSceneObject* sceneObject, const std::shared_ptr<WiVeBVH8Build8<BotLevelLeafNodeInstanced>>& bvh);

        Bounds getBounds() const;
        Bounds worldBounds() const;
        bool intersect(Ray& ray, RayHit& hitInfo) const;

    private:
        const InCoreGeometricSceneObject* m_sceneObject;
        unsigned m_primitiveID;
    };

    class TopLevelLeafNode {
    public:
        TopLevelLeafNode(
            gsl::span<BotLevelLeafNode> leafs,
            InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);
        TopLevelLeafNode(TopLevelLeafNode&& other);

        Bounds getBounds() const;

        std::optional<bool> intersect(Ray& ray, RayHit& rayHit, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.
        std::optional<bool> intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const; // Batches rays. This function is thread safe.

        // Flush a whole range of nodes at a time as opposed to a non-static flush member function which would require a
        // separate tbb flow graph for each node that is processed.
        static bool flushRange(gsl::span<TopLevelLeafNode*> nodes, InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

        static void compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes);

        size_t sizeBytes() const;

    private:
        struct SVDAGRayOffset {
            glm::vec3 gridBoundsMin;
            glm::vec3 invGridBoundsExtent;
        };
        static std::pair<SparseVoxelDAG, SVDAGRayOffset> computeSVDAG(gsl::span<const InCoreSceneObject*> sceneObjects);
        static Bounds combinedBounds(gsl::span<const BotLevelLeafNode> sceneObjects);

    private:
        WiVeBVH8Build8<BotLevelLeafNode> m_bvh;
        Bounds m_bounds;

        int m_numFullBatches;
        RayBatch* m_batch;
        GrowingFreeListTS<RayBatch>* m_batchAllocator;
        //InCoreBatchingAccelerationStructure<UserState, BatchSize>* m_accelerationStructurePtr;// Used for its ray batch allocator

        //std::pair<SparseVoxelDAG, SVDAGRayOffset> m_svdagAndTransform;
    };

    static PauseableBVH4<TopLevelLeafNode, UserState> buildBVH(
        const Scene& scene,
        InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr);

private:
    GrowingFreeListTS<RayBatch> m_batchAllocator;
    //tbb::scalable_allocator<RayBatch> m_batchAllocator;

    PauseableBVH4<TopLevelLeafNode, UserState> m_bvh;

    HitCallback m_hitCallback;
    AnyHitCallback m_anyHitCallback;
    MissCallback m_missCallback;
};

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::InCoreBatchingAccelerationStructure(
    const Scene& scene,
    HitCallback hitCallback, AnyHitCallback anyHitCallback, MissCallback missCallback)
    : m_batchAllocator()
    , m_bvh(std::move(buildBVH(scene, this)))
    , m_hitCallback(hitCallback)
    , m_anyHitCallback(anyHitCallback)
    , m_missCallback(missCallback)
{
    g_stats.memory.topBVH += m_bvh.sizeBytes();
    for (const auto* leaf : m_bvh.leafs()) {
        g_stats.memory.topBVHLeafs += leaf->sizeBytes();
    }
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
        Ray ray = rays[i]; // Copy so we can mutate it
        RayHit rayHit;
        UserState userState = perRayUserData[i]; // Copy so we can move items to a batch

        auto optResult = m_bvh.intersect(ray, rayHit, userState);
        if (optResult && *optResult == false) {
            // If we get a result directly it must be because we missed the scene
            m_missCallback(ray, userState);
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
        Ray ray = rays[i];
        UserState userState = perRayUserData[i]; // Copy so we can mutate it

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
inline PauseableBVH4<typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode, UserState> InCoreBatchingAccelerationStructure<UserState, BatchSize>::buildBVH(
    const Scene& scene,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructure)
{
    std::vector<BotLevelLeafNode> botLeafs;
    for (const auto* sceneObject : scene.getInCoreSceneObjects()) {
        if (const auto* geometricSceneObject = dynamic_cast<const InCoreGeometricSceneObject*>(sceneObject)) {
            // Create bot-level leaf node
            for (unsigned primitiveID = 0; primitiveID < geometricSceneObject->numPrimitives(); primitiveID++) {
                botLeafs.push_back(BotLevelLeafNode(geometricSceneObject, primitiveID));
            }
        } else if (const auto* instancedSceneObject = dynamic_cast<const InCoreInstancedSceneObject*>(sceneObject)) {
            std::cerr << "INSTANCING NOT SUPPORTED" << std::endl;
        }
    }

    std::vector<TopLevelLeafNode> leafs;
    for (auto botLeafsGroup : geometricGroupObjects<BotLevelLeafNode>(botLeafs, IN_CORE_BATCHING_PRIMS_PER_LEAF)) {
        leafs.emplace_back(std::move(TopLevelLeafNode(botLeafsGroup, accelerationStructure)));
    }
    return PauseableBVH4<TopLevelLeafNode, UserState>(leafs);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(
    gsl::span<BotLevelLeafNode> leafs,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructure)
    : m_bvh(leafs)
    , m_bounds(combinedBounds(leafs))
    , m_numFullBatches(0)
    , m_batch(nullptr)
    , m_batchAllocator(&accelerationStructure->m_batchAllocator)
{
    size_t size = sizeof(TopLevelLeafNode);
    size += m_bvh.sizeBytes();
    std::cout << "Approximate TopLeveLeafNode size (exc mesh): " << size << std::endl;
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::TopLevelLeafNode(TopLevelLeafNode&& other)
    : m_bvh(std::move(other.m_bvh))
    , m_bounds(std::move(other.m_bounds))
    , m_numFullBatches(0)
    , m_batch(other.m_batch)
    , m_batchAllocator(other.m_batchAllocator)
{
}

template <typename UserState, size_t BatchSize>
inline std::pair<SparseVoxelDAG, typename InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::SVDAGRayOffset> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::computeSVDAG(gsl::span<const InCoreSceneObject*> sceneObjects)
{
    Bounds gridBounds;
    for (const auto* sceneObject : sceneObjects) {
        gridBounds.extend(sceneObject->worldBounds());
    }

    VoxelGrid voxelGrid(IN_CORE_SVDAG_RESOLUTION);
    for (const auto* sceneObject : sceneObjects) {
        sceneObject->voxelize(voxelGrid, gridBounds);
    }

    // SVO is at (1, 1, 1) to (2, 2, 2)
    float maxDim = maxComponent(gridBounds.extent());

    SparseVoxelDAG svdag(voxelGrid);
    return { std::move(svdag), SVDAGRayOffset { gridBounds.min, glm::vec3(1.0f / maxDim) } };
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::combinedBounds(gsl::span<const BotLevelLeafNode> leafs)
{
    Bounds ret;
    for (const auto& leaf : leafs) {
        ret.extend(leaf.worldBounds());
    }
    return ret;
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::getBounds() const
{
    return m_bounds;
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersect(
    Ray& ray,
    RayHit& rayHit,
    const UserState& userState,
    PauseableBVHInsertHandle insertHandle) const
{
    if constexpr (IN_CORE_OCCLUSION_CULLING) {
        /*{
            //auto scopedTimings = g_stats.timings.svdagTraversalTime.getScopedStopwatch();

            const auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
            auto svdagRay = ray;
            svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
            if (!svdag.intersectScalar(svdagRay))
                return false; // Missed, continue traversal
        }*/
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    if (!m_batch || m_batch->full()) {
        if (m_batch) {
            mutThisPtr->m_numFullBatches += 1;
        }

        // Allocate a new batch and set it as the new active batch
        auto* mem = mutThisPtr->m_batchAllocator->allocate();
        mutThisPtr->m_batch = new (mem) RayBatch(m_batch);
    }

    auto& mutUserState = const_cast<UserState&>(userState);
    mutThisPtr->m_batch->tryPush(ray, rayHit, std::move(mutUserState), insertHandle);

    return {}; // Paused
}

template <typename UserState, size_t BatchSize>
inline std::optional<bool> InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::intersectAny(Ray& ray, const UserState& userState, PauseableBVHInsertHandle insertHandle) const
{
    if constexpr (IN_CORE_OCCLUSION_CULLING) {
        /*auto& [svdag, svdagRayOffset] = m_svdagAndTransform;
        auto svdagRay = ray;
        svdagRay.origin = glm::vec3(1.0f) + (svdagRayOffset.invGridBoundsExtent * (ray.origin - svdagRayOffset.gridBoundsMin));
        if (!svdag.intersectScalar(svdagRay))
            return false; // Missed, continue traversal*/
    }

    auto* mutThisPtr = const_cast<TopLevelLeafNode*>(this);

    if (!m_batch || m_batch->full()) {
        if (m_batch) {
            mutThisPtr->m_numFullBatches += 1;
        }

        // Allocate a new batch and set it as the new active batch
        auto* mem = mutThisPtr->m_batchAllocator->allocate();
        mutThisPtr->m_batch = new (mem) RayBatch(m_batch);
    }

    auto& mutUserState = const_cast<UserState&>(userState);
    mutThisPtr->m_batch->tryPush(ray, std::move(mutUserState), insertHandle);

    return {}; // Paused
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::flush()
{
    while (TopLevelLeafNode::flushRange(m_bvh.leafs(), this)) {
    }

    std::cout << "FLUSH COMPLETE" << std::endl;
}

template <typename UserState, size_t BatchSize>
bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::flushRange(
    gsl::span<TopLevelLeafNode*> nodes,
    InCoreBatchingAccelerationStructure<UserState, BatchSize>* accelerationStructurePtr)
{
    std::sort(std::begin(nodes), std::end(nodes), [](const auto* node1, const auto* node2) {
        return node1->m_numFullBatches > node2->m_numFullBatches; // Sort from big to small
    });

    for (auto* node : nodes) {
        // Flushing batches may insert new rays so make sure that we reset the linked list first
        auto* firstBatch = node->m_batch;
        node->m_batch = nullptr;
        node->m_numFullBatches = 0;

        auto* batch = firstBatch;
        while (batch) {
            for (auto [ray, rayHitOpt, userState, insertHandle] : *batch) {
                // Intersect with the bottom-level BVH
                if (rayHitOpt) {
                    node->m_bvh.intersect(gsl::make_span(&ray, 1), gsl::make_span(&(*rayHitOpt), 1));
                } else {
                    node->m_bvh.intersectAny(gsl::make_span(&ray, 1));
                }
            }

            batch = batch->next();
        }

        batch = firstBatch;
        while (batch) {
            for (auto [ray, rayHitOpt, userState, insertHandle] : *batch) {
                if (rayHitOpt) {
                    // Insert the ray back into the top-level  BVH
                    auto optResult = accelerationStructurePtr->m_bvh.intersect(ray, *rayHitOpt, userState, insertHandle);
                    if (optResult && *optResult == false) {
                        // Ray exited the system so we can run the hit/miss shaders
                        const auto* hitSceneObject = std::get<const InCoreSceneObject*>(rayHitOpt->sceneObjectVariant);
                        if (hitSceneObject) {
                            auto si = hitSceneObject->fillSurfaceInteraction(ray, *rayHitOpt);
                            si.sceneObjectMaterial = hitSceneObject;
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

            accelerationStructurePtr->m_batchAllocator.deallocate(batch);
            batch = batch->next();
        }
    }

    return std::any_of(std::begin(nodes), std::end(nodes), [](const auto* node) -> bool {
        return node->m_batch;
    });
}

template <typename UserState, size_t BatchSize>
inline void InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::compressSVDAGs(gsl::span<TopLevelLeafNode*> nodes)
{
    /*std::vector<SparseVoxelDAG*> dags;
    for (auto* node : nodes) {
        dags.push_back(&std::get<0>(node->m_svdagAndTransform));
    }
    SparseVoxelDAG::compressDAGs(dags);

    for (const auto* dag : dags) {
        g_stats.memory.svdags += dag->sizeBytes();
    }*/
}

template <typename UserState, size_t BatchSize>
inline size_t InCoreBatchingAccelerationStructure<UserState, BatchSize>::TopLevelLeafNode::sizeBytes() const
{
    size_t size = sizeof(decltype(*this));
    //size += std::get<0>(m_svdagAndTransform).sizeBytes();
    return size;
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::BotLevelLeafNodeInstanced(
    const InCoreGeometricSceneObject* baseSceneObject,
    unsigned primitiveID)
    : m_baseSceneObject(baseSceneObject)
    , m_primitiveID(primitiveID)
{
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::getBounds() const
{
    return m_baseSceneObject->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced::intersect(Ray& ray, RayHit& hitInfo) const
{
    return m_baseSceneObject->intersectPrimitive(ray, hitInfo, m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const InCoreGeometricSceneObject* sceneObject,
    unsigned primitiveID)
    : m_sceneObject(sceneObject)
    , m_primitiveID(primitiveID)
{
}

/*template <typename UserState, size_t BatchSize>
inline InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::BotLevelLeafNode(
    const InCoreInstancedSceneObject* sceneObject,
    const std::shared_ptr<WiVeBVH8Build8<InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNodeInstanced>>& bvh)
    : m_data(Instance { sceneObject, bvh })
{
}*/

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::getBounds() const
{
    return m_sceneObject->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline Bounds InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::worldBounds() const
{
    return m_sceneObject->worldBoundsPrimitive(m_primitiveID);
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::BotLevelLeafNode::intersect(Ray& ray, RayHit& hitInfo) const
{
    bool hit = m_sceneObject->intersectPrimitive(ray, hitInfo, m_primitiveID);
    if (hit) {
        hitInfo.sceneObjectVariant = dynamic_cast<const InCoreSceneObject*>(m_sceneObject);
    }
    return hit;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, const RayHit& rayHit, UserState&& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, rayHit, std::move(state), insertHandle);

    return true;
}

template <typename UserState, size_t BatchSize>
inline bool InCoreBatchingAccelerationStructure<UserState, BatchSize>::RayBatch::tryPush(const Ray& ray, UserState&& state, const PauseableBVHInsertHandle& insertHandle)
{
    m_data.emplace_back(ray, std::move(state), insertHandle);

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
    auto& [ray, rayHit, userState, insertHandle] = m_rayBatch->m_data[m_index];
    return { ray, rayHit, userState, insertHandle };
}
}
