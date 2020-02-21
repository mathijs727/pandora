#pragma once
#include "pandora/core/stats.h"
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/samplers/rng/pcg.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/traversal/acceleration_structure.h"
#include "pandora/traversal/batching.h"
#include "pandora/traversal/offline_bvh_cache.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/traversal/sub_scene.h"
#include "pandora/utility/enumerate.h"
#include "stream/cache/lru_cache.h"
#include "stream/task_graph.h"
#include <glm/gtc/type_ptr.hpp>
#include <gsl/span>
#include <optick.h>
#include <optional>
#include <stream/cache/evictable.h>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace pandora {

template <typename HitRayState, typename AnyHitRayState>
class OfflineBatchingAccelerationStructure : public AccelerationStructure<HitRayState, AnyHitRayState> {
public:
    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

private:
    friend class OfflineBatchingAccelerationStructureBuilder;
    class BatchingPoint;
    OfflineBatchingAccelerationStructure(
        PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>&& topLevelBVH,
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
        tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph, LRUBVHSceneCache&& bvhSceneCache);

    using TopLevelBVH = PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>;
    using OnHitTask = tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>>;
    using OnMissTask = tasking::TaskHandle<std::tuple<Ray, HitRayState>>;
    using OnAnyHitTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;
    using OnAnyMissTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;

    class BatchingPoint {
    public:
        BatchingPoint(std::unique_ptr<SubScene>&& pSubScene, std::vector<Shape*>&& shapes, SparseVoxelDAG&& svdag, tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph);
        BatchingPoint(std::unique_ptr<SubScene>&& pSubScene, std::vector<Shape*>&& shapes, tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph);

        std::optional<bool> intersect(Ray&, SurfaceInteraction&, const HitRayState&, const PauseableBVHInsertHandle&) const;
        std::optional<bool> intersectAny(Ray&, const AnyHitRayState&, const PauseableBVHInsertHandle&) const;

        Bounds getBounds() const;

    private:
        friend class OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>;
        void setParent(OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent, LRUBVHSceneCache* pBVHCache);

        static inline glm::vec3 randomVec3()
        {
            static PcgRng rng { 94892380 };
            return rng.uniformFloat3();
        }

        struct StaticData {
            std::vector<tasking::CachedPtr<Shape>> shapeOwners;

            CachedBVHSubScene bvhSubScene;
        };

    private:
        std::unique_ptr<SubScene> m_pSubScene;
        std::vector<Shape*> m_shapes;
        Bounds m_bounds;
        glm::vec3 m_color;

        std::optional<SparseVoxelDAG> m_svdag;

        tasking::LRUCache* m_pGeometryCache;
        LRUBVHSceneCache* m_pBVHCache;
        tasking::TaskGraph* m_pTaskGraph;

        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> m_intersectTask;
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> m_intersectAnyTask;
    };

private:
    TopLevelBVH m_topLevelBVH;
    LRUBVHSceneCache m_bvhSceneCache;

    bool m_enableOcclusionCulling;
    tasking::TaskGraph* m_pTaskGraph;

    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

class OfflineBatchingAccelerationStructureBuilder {
public:
    OfflineBatchingAccelerationStructureBuilder(
        const Scene* pScene, tasking::LRUCache* pCache, tasking::TaskGraph* pTaskGraph, unsigned primitivesPerBatchingPoint, size_t botLevelBVHCacheSize, unsigned svdagRes);

    static void preprocessScene(Scene& scene, tasking::LRUCache& oldCache, tasking::CacheBuilder& newCacheBuilder, unsigned primitivesPerBatchingPoint);

    template <typename HitRayState, typename AnyHitRayState>
    OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    const size_t m_botLevelBVHCacheSize;
    const unsigned m_svdagRes;

    std::vector<std::unique_ptr<SubScene>> m_subScenes;

    tasking::LRUCache* m_pGeometryCache;
    tasking::TaskGraph* m_pTaskGraph;
};

template <typename HitRayState, typename AnyHitRayState>
OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    std::unique_ptr<SubScene>&& pSubScene, std::vector<Shape*>&& shapes, SparseVoxelDAG&& svdag, tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph)
    : m_pSubScene(std::move(pSubScene))
    , m_shapes(std::move(shapes))
    , m_bounds(m_pSubScene->computeBounds())
    , m_color(randomVec3())
    , m_svdag(std::move(svdag))
    , m_pGeometryCache(pGeometryCache)
    , m_pTaskGraph(pTaskGraph)
{
}

template <typename HitRayState, typename AnyHitRayState>
OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    std::unique_ptr<SubScene>&& pSubScene, std::vector<Shape*>&& shapes, tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph)
    : m_pSubScene(std::move(pSubScene))
    , m_shapes(std::move(shapes))
    , m_bounds(m_pSubScene->computeBounds())
    , m_color(randomVec3())
    , m_pGeometryCache(pGeometryCache)
    , m_pTaskGraph(pTaskGraph)
{
}

template <typename HitRayState, typename AnyHitRayState>
void OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::setParent(
    OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent, LRUBVHSceneCache* pBVHCache)
{
    //m_pParent = pParent;
    m_intersectTask = m_pTaskGraph->addTask<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>, StaticData>(
        "OfflineBatchingAccelerationStructure::leafIntersect",
        [=]() -> StaticData {
            //g_stats.memory.batches = m_pTaskGraph->approxMemoryUsage();

            std::vector<tasking::CachedPtr<Shape>> shapeOwners;
            {
                OPTICK_EVENT("MakeShapesResident");
                for (Shape* pShape : m_shapes) {
                    shapeOwners.emplace_back(m_pGeometryCache->makeResident(pShape));
                }
            }

            std::optional<CachedBVHSubScene> optSubSceneBVH;
            {
                OPTICK_EVENT("LoadOrBuildBVH");
                optSubSceneBVH = pBVHCache->fromSubScene(m_pSubScene.get());
            }

            return StaticData { std::move(shapeOwners), std::move(*optSubSceneBVH) };
        },
        [=](gsl::span<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> data, const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            {
                auto stopWatch = g_stats.timings.botLevelTraversalTime.getScopedStopwatch();

                for (auto& [ray, si, state, insertHandle] : data) {
                    pStaticData->bvhSubScene.intersect(ray, si);
                }
            }

            {
                auto stopWatch = g_stats.timings.topLevelTraversalTime.getScopedStopwatch();

                for (auto& [ray, si, state, insertHandle] : data) {
                    auto optHit = pParent->m_topLevelBVH.intersect(ray, si, state, insertHandle);
                    if (optHit) { // Ray exited BVH
                        assert(!optHit.value());

                        if (si.pSceneObject) {
                            // Ray hit something
                            assert(si.pSceneObject);
                            m_pTaskGraph->enqueue(pParent->m_onHitTask, std::tuple { ray, si, state });
                        } else {
                            m_pTaskGraph->enqueue(pParent->m_onMissTask, std::tuple { ray, state });
                        }
                    }
                }
            }
        });
    m_intersectAnyTask = m_pTaskGraph->addTask<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>, StaticData>(
        "OfflineBatchingAccelerationStructure::leafIntersectAny",
        [=]() -> StaticData {
            std::vector<tasking::CachedPtr<Shape>> shapeOwners;
            {
                OPTICK_EVENT("MakeShapesResident");
                for (Shape* pShape : m_shapes) {
                    shapeOwners.emplace_back(m_pGeometryCache->makeResident(pShape));
                }
            }

            std::optional<CachedBVHSubScene> optSubSceneBVH;
            {
                OPTICK_EVENT("LoadOrBuildBVH");
                optSubSceneBVH = pBVHCache->fromSubScene(m_pSubScene.get());
            }

            return StaticData { std::move(shapeOwners), std::move(*optSubSceneBVH) };
        },
        [=](gsl::span<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> data, const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            std::vector<uint32_t> hits;
            hits.resize(data.size());
            std::fill(std::begin(hits), std::end(hits), false);

            {
                auto stopWatch = g_stats.timings.botLevelTraversalTime.getScopedStopwatch();

                for (auto&& [i, data] : enumerate(data)) {
                    auto& [ray, state, insertHandle] = data;
                    hits[i] = pStaticData->bvhSubScene.intersectAny(ray);
                }
            }

            {
                auto stopWatch = g_stats.timings.topLevelTraversalTime.getScopedStopwatch();

                for (auto&& [i, data] : enumerate(data)) {
                    auto& [ray, state, insertHandle] = data;

                    if (hits[i]) {
                        m_pTaskGraph->enqueue(pParent->m_onAnyHitTask, std::tuple { ray, state });
                    } else {
                        auto optHit = pParent->m_topLevelBVH.intersectAny(ray, state, insertHandle);
                        if (optHit) {
                            assert(!optHit.value());
                            if (optHit.value())
                                m_pTaskGraph->enqueue(pParent->m_onAnyHitTask, std::tuple { ray, state });
                            else
                                m_pTaskGraph->enqueue(pParent->m_onAnyMissTask, std::tuple { ray, state });
                        }
                    }
                }
            }
        });
}

template <typename HitRayState, typename AnyHitRayState>
Bounds OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::getBounds() const
{
    return m_bounds;
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersect(
    Ray& ray, SurfaceInteraction& si, const HitRayState& userState, const PauseableBVHInsertHandle& bvhInsertHandle) const
{
    if (m_svdag) {
        // auto stopWatch = g_stats.timings.svdagTraversalTime.getScopedStopwatch();
        g_stats.svdag.numIntersectionTests++;

        if (!m_svdag->intersectScalar(ray)) {
            g_stats.svdag.numRaysCulled++;
            return false;
        }
    }

    ray.numTopLevelIntersections += 1;
    m_pTaskGraph->enqueue(m_intersectTask, std::tuple { ray, si, userState, bvhInsertHandle });
    return {};
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAny(
    Ray& ray, const AnyHitRayState& userState, const PauseableBVHInsertHandle& bvhInsertHandle) const
{
    if (m_svdag) {
        //auto stopWatch = g_stats.timings.svdagTraversalTime.getScopedStopwatch();
        g_stats.svdag.numIntersectionTests++;

        if (!m_svdag->intersectScalar(ray)) {
            g_stats.svdag.numRaysCulled++;
            return false;
        }
    }

    ray.numTopLevelIntersections += 1;
    m_pTaskGraph->enqueue(m_intersectAnyTask, std::tuple { ray, userState, bvhInsertHandle });
    return {};
}

template <typename HitRayState, typename AnyHitRayState>
inline OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState> OfflineBatchingAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    OPTICK_EVENT();

    spdlog::info("Constructing bot level BVHs");
    // From vector of unique pointers to vector of raw pointers
    std::vector<const SubScene*> subScenes;
    std::transform(std::begin(m_subScenes), std::end(m_subScenes), std::back_inserter(subScenes), [](const auto& subScene) { return subScene.get(); });
    LRUBVHSceneCache sceneCache { subScenes, m_pGeometryCache, m_botLevelBVHCacheSize };

    spdlog::info("Creating batching points");
    using BatchingPointT = typename OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint;
    std::vector<BatchingPointT> batchingPoints;
    if (m_svdagRes > 0) {
        std::vector<std::optional<SparseVoxelDAG>> svdags;
        svdags.resize(m_subScenes.size());

        // TODO: make cache thread safe and update this code
        // Because the caches are not thread safe (yet) we have to load the data from the main thread..
        // We keep track of how many threads are working so that we never load new data faster than we can process it.
        const unsigned maxParallelism = std::max(1u, std::thread::hardware_concurrency() - 1);
        std::atomic_uint parallelTasks { 0 };

        spdlog::info("Voxelizing geometry");
        tbb::task_group tg;
        for (size_t i = 0; i < m_subScenes.size(); i++) {
            while (parallelTasks.load(std::memory_order::memory_order_acquire) >= maxParallelism)
                continue;

            // Make resident sequentially
            const auto& subScene = *m_subScenes[i];
            auto shapesOwningPtrs = detail::makeSubSceneResident(subScene, *m_pGeometryCache);

            // Voxelize and create SVO in parallel
            parallelTasks.fetch_add(1);
            tg.run([i, &subScene, shapesOwningPtrs = std::move(shapesOwningPtrs), &svdags, &parallelTasks, this]() {
                svdags[i] = detail::createSVDAGfromSubScene(subScene, m_svdagRes);
                parallelTasks.fetch_sub(1);
            });
        }
        tg.wait();

        for (const auto& svdag : svdags)
            g_stats.memory.svdagsBeforeCompression += svdag->sizeBytes();

        spdlog::info("Compressing SVO to SVDAGs");
        std::vector<SparseVoxelDAG*> pSvdags;
        for (auto& svdag : svdags)
            pSvdags.push_back(&svdag.value());
        SparseVoxelDAG::compressDAGs(pSvdags);

        for (const auto& svdag : svdags)
            g_stats.memory.svdagsAfterCompression += svdag->sizeBytes();

        for (size_t i = 0; i < m_subScenes.size(); i++) {
            auto& pSubScene = m_subScenes[i];
            auto shapes = detail::getSubSceneShapes(*pSubScene);
            batchingPoints.emplace_back(std::move(pSubScene), std::move(shapes), std::move(*svdags[i]), m_pGeometryCache, m_pTaskGraph);
        }
    } else {
        for (auto& pSubScene : m_subScenes) {
            auto shapes = detail::getSubSceneShapes(*pSubScene);
            batchingPoints.emplace_back(std::move(pSubScene), std::move(shapes), m_pGeometryCache, m_pTaskGraph);
        }
    }

    spdlog::info("Constructing top level BVH over {} batching points", batchingPoints.size());

    // Moves batching points into internal structure
    PauseableBVH4<BatchingPointT, HitRayState, AnyHitRayState> topLevelBVH { batchingPoints };
    g_stats.scene.numBatchingPoints = batchingPoints.size();
    g_stats.memory.topBVH = topLevelBVH.sizeBytes();
    g_stats.memory.topBVHLeafs = batchingPoints.size() * sizeof(BatchingPointT);
    spdlog::info("PausableBVH constructed");
    return OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>(
        std::move(topLevelBVH), hitTask, missTask, anyHitTask, anyMissTask, m_pGeometryCache, m_pTaskGraph, std::move(sceneCache));
}

template <typename HitRayState, typename AnyHitRayState>
inline OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::OfflineBatchingAccelerationStructure(
    PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>&& topLevelBVH,
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::LRUCache* pGeometryCache, tasking::TaskGraph* pTaskGraph, LRUBVHSceneCache&& bvhSceneCache)
    : m_topLevelBVH(std::move(topLevelBVH))
    , m_pTaskGraph(pTaskGraph)
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
    , m_bvhSceneCache(std::move(bvhSceneCache))
{
    for (auto& leaf : m_topLevelBVH.leafs())
        leaf.setParent(this, &m_bvhSceneCache);
}

template <typename HitRayState, typename AnyHitRayState>
inline void OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
{
    auto stopWatch = g_stats.timings.topLevelTraversalTime.getScopedStopwatch();

    auto mutRay = ray;
    SurfaceInteraction si;
    auto optHit = m_topLevelBVH.intersect(mutRay, si, state);
    if (optHit) {
        assert(!optHit.value());
        if (optHit.value())
            m_pTaskGraph->enqueue(m_onHitTask, std::tuple { mutRay, si, state });
        else
            m_pTaskGraph->enqueue(m_onMissTask, std::tuple { mutRay, state });
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline void OfflineBatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
{
    auto stopWatch = g_stats.timings.topLevelTraversalTime.getScopedStopwatch();

    auto mutRay = ray;
    auto optHit = m_topLevelBVH.intersectAny(mutRay, state);
    if (optHit) {
        assert(!optHit.value());
        if (optHit.value())
            m_pTaskGraph->enqueue(m_onAnyHitTask, std::tuple { mutRay, state });
        else
            m_pTaskGraph->enqueue(m_onAnyMissTask, std::tuple { mutRay, state });
    }
}
}