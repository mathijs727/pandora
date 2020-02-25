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
#include "pandora/traversal/embree_cache.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "pandora/utility/enumerate.h"
#include <embree3/rtcore.h>
#include <execution>
#include <glm/gtc/type_ptr.hpp>
#include <gsl/span>
#include <optional>
#include <spdlog/spdlog.h>
#include <stream/cache/lru_cache.h>
#include <stream/cache/lru_cache_ts.h>
#include <stream/task_graph.h>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pandora {

class BatchingAccelerationStructureBuilder;

template <typename HitRayState, typename AnyHitRayState>
class BatchingAccelerationStructure : public AccelerationStructure<HitRayState, AnyHitRayState> {
public:
    ~BatchingAccelerationStructure();

    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

    std::optional<SurfaceInteraction> intersectDebug(Ray& ray) const;

private:
    friend class BatchingAccelerationStructureBuilder;
    class BatchingPoint;
    BatchingAccelerationStructure(
        RTCDevice embreeDevice, PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>&& topLevelBVH,
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
        tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph, size_t embreeSceneCacheSize);

    using TopLevelBVH = PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>;
    using OnHitTask = tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>>;
    using OnMissTask = tasking::TaskHandle<std::tuple<Ray, HitRayState>>;
    using OnAnyHitTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;
    using OnAnyMissTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;

    class BatchingPoint {
    public:
        BatchingPoint(SubScene&& subScene, std::vector<Shape*>&& shapes, SparseVoxelDAG&& svdag, tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph);
        BatchingPoint(SubScene&& subScene, std::vector<Shape*>&& shapes, tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph);

        std::optional<bool> intersect(Ray&, SurfaceInteraction&, const HitRayState&, const PauseableBVHInsertHandle&) const;
        std::optional<bool> intersectAny(Ray&, const AnyHitRayState&, const PauseableBVHInsertHandle&) const;

        Bounds getBounds() const;

    private:
        bool intersectInternal(RTCScene scene, Ray&, SurfaceInteraction&) const;
        bool intersectAnyInternal(RTCScene scene, Ray&) const;

        friend class BatchingAccelerationStructure<HitRayState, AnyHitRayState>;
        void setParent(BatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent, EmbreeSceneCache* pEmbreeCache);

        struct StaticData {
            std::vector<tasking::CachedPtr<Shape>> shapeOwners;

            std::shared_ptr<CachedEmbreeScene> scene;
        };

    private:
        SubScene m_subScene;
        std::vector<Shape*> m_shapes;
        Bounds m_bounds;
        glm::vec3 m_color;

        std::optional<SparseVoxelDAG> m_svdag;

        tasking::LRUCacheTS* m_pGeometryCache;
        EmbreeSceneCache* m_pEmbreeCache;
        tasking::TaskGraph* m_pTaskGraph;

        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> m_intersectTask;
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> m_intersectAnyTask;
    };

private:
    RTCDevice m_embreeDevice;
    TopLevelBVH m_topLevelBVH;
    LRUEmbreeSceneCache m_embreeSceneCache;

    bool m_enableOcclusionCulling;
    tasking::TaskGraph* m_pTaskGraph;

    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

class BatchingAccelerationStructureBuilder {
public:
    BatchingAccelerationStructureBuilder(
        const Scene* pScene, tasking::LRUCacheTS* pCache, tasking::TaskGraph* pTaskGraph, unsigned primitivesPerBatchingPoint, size_t botLevelBVHCacheSize, unsigned svdagRes);

    static void preprocessScene(Scene& scene, tasking::LRUCacheTS& oldCache, tasking::CacheBuilder& newCacheBuilder, unsigned primitivesPerBatchingPoint);

    template <typename HitRayState, typename AnyHitRayState>
    BatchingAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    const size_t m_botLevelBVHCacheSize;
    const unsigned m_svdagRes;

    RTCDevice m_embreeDevice;
    std::vector<SubScene> m_subScenes;

    tasking::LRUCacheTS* m_pGeometryCache;
    tasking::TaskGraph* m_pTaskGraph;
};

inline glm::vec3 randomVec3()
{
    static PcgRng rng { 94892380 };
    return rng.uniformFloat3();
}

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    SubScene&& subScene, std::vector<Shape*>&& shapes, SparseVoxelDAG&& svdag, tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph)
    : m_subScene(std::move(subScene))
    , m_shapes(std::move(shapes))
    , m_bounds(m_subScene.computeBounds())
    , m_color(randomVec3())
    , m_svdag(std::move(svdag))
    , m_pGeometryCache(pGeometryCache)
    , m_pTaskGraph(pTaskGraph)
{
}

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    SubScene&& subScene, std::vector<Shape*>&& shapes, tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph)
    : m_subScene(std::move(subScene))
    , m_shapes(std::move(shapes))
    , m_bounds(m_subScene.computeBounds())
    , m_color(randomVec3())
    , m_pGeometryCache(pGeometryCache)
    , m_pTaskGraph(pTaskGraph)
{
}

template <typename HitRayState, typename AnyHitRayState>
void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::setParent(
    BatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent, EmbreeSceneCache* pEmbreeCache)
{
    //m_pParent = pParent;
    m_intersectTask = m_pTaskGraph->addTask<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>, StaticData>(
        "BatchingAccelerationStructure::leafIntersect",
        [=]() -> StaticData {
            //g_stats.memory.batches = m_pTaskGraph->approxMemoryUsage();

            StaticData staticData;
            {
                OPTICK_EVENT("MakeShapesResident");
                 staticData.shapeOwners.reserve(m_shapes.size());
                for (Shape* pShape : m_shapes) {
                    auto shapeOwner = m_pGeometryCache->makeResident(pShape);
                    staticData.shapeOwners.emplace_back(std::move(shapeOwner));
                }
            }

            {
                OPTICK_EVENT("LoadOrBuildBVH");
                staticData.scene = pEmbreeCache->fromSubScene(&m_subScene);
            }

            return staticData;
        },
        [=](gsl::span<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> data,
            const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            {
                auto stopWatch = g_stats.timings.botLevelTraversalTime.getScopedStopwatch();

                RTCScene embreeScene = pStaticData->scene->scene;
                for (auto& [ray, si, state, insertHandle] : data) {
                    intersectInternal(embreeScene, ray, si);
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
        "BatchingAccelerationStructure::leafIntersectAny",
        [=]() -> StaticData {
            StaticData staticData;

            {
                OPTICK_EVENT("MakeShapesResident");
                staticData.shapeOwners.reserve(m_shapes.size());
                for (Shape* pShape : m_shapes) {
                    auto shapeOwner = m_pGeometryCache->makeResident(pShape);
                    staticData.shapeOwners.emplace_back(std::move(shapeOwner));
                }
            }

            {
                OPTICK_EVENT("LoadOrBuildBVH");
                staticData.scene = pEmbreeCache->fromSubScene(&m_subScene);
            }

            return staticData;
        },
        [=](gsl::span<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> data, const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            std::vector<uint32_t> hits;
            hits.resize(data.size());
            std::fill(std::begin(hits), std::end(hits), false);

            {
                auto stopWatch = g_stats.timings.botLevelTraversalTime.getScopedStopwatch();

                RTCScene embreeScene = pStaticData->scene->scene;
                for (auto&& [i, data] : enumerate(data)) {
                    auto& [ray, state, insertHandle] = data;
                    hits[i] = intersectAnyInternal(embreeScene, ray);
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
Bounds BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::getBounds() const
{
    return m_bounds;
}

template <typename HitRayState, typename AnyHitRayState>
inline std::optional<SurfaceInteraction> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectDebug(Ray& ray) const
{
    SurfaceInteraction si;
    if (m_topLevelBVH.intersect(ray, si, HitRayState {}))
        return si;
    else
        return {};
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersect(
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
std::optional<bool> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAny(
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
bool BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectInternal(
    RTCScene scene, Ray& ray, SurfaceInteraction& si) const
{
    RTCRayHit embreeRayHit;
    embreeRayHit.ray.org_x = ray.origin.x;
    embreeRayHit.ray.org_y = ray.origin.y;
    embreeRayHit.ray.org_z = ray.origin.z;
    embreeRayHit.ray.dir_x = ray.direction.x;
    embreeRayHit.ray.dir_y = ray.direction.y;
    embreeRayHit.ray.dir_z = ray.direction.z;

    embreeRayHit.ray.tnear = ray.tnear;
    embreeRayHit.ray.tfar = ray.tfar;

    embreeRayHit.ray.time = 0.0f;
    embreeRayHit.ray.mask = 0xFFFFFFFF;
    embreeRayHit.ray.id = 0;
    embreeRayHit.ray.flags = 0;
    embreeRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++)
        embreeRayHit.hit.instID[i] = RTC_INVALID_GEOMETRY_ID;
    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    rtcIntersect1(scene, &context, &embreeRayHit);

    static constexpr float minInf = -std::numeric_limits<float>::infinity();
    if (embreeRayHit.ray.tfar == minInf || embreeRayHit.ray.tfar == ray.tfar)
        return false;

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        std::optional<glm::mat4> optLocalToWorldMatrix;
        const SceneObject* pSceneObject { nullptr };

        if (embreeRayHit.hit.instID[0] == RTC_INVALID_GEOMETRY_ID) {
            pSceneObject = reinterpret_cast<const SceneObject*>(
                TriangleShape::getAdditionalUserData(rtcGetGeometry(scene, embreeRayHit.hit.geomID)));
        } else {
            glm::mat4 accumulatedTransform { 1.0f };
            RTCScene localScene = scene;
            for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
                unsigned geomID = embreeRayHit.hit.instID[i];
                if (geomID == RTC_INVALID_GEOMETRY_ID)
                    break;

                RTCGeometry geometry = rtcGetGeometry(localScene, geomID);

                glm::mat4 localTransform;
                rtcGetGeometryTransform(geometry, 0.0f, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, glm::value_ptr(localTransform));
                accumulatedTransform *= localTransform;

                localScene = reinterpret_cast<RTCScene>(rtcGetGeometryUserData(geometry));
            }

            optLocalToWorldMatrix = accumulatedTransform;
            pSceneObject = reinterpret_cast<const SceneObject*>(
                TriangleShape::getAdditionalUserData(rtcGetGeometry(localScene, embreeRayHit.hit.geomID)));
        }

        RayHit hit;
        hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
        hit.geometricNormal = glm::normalize(hit.geometricNormal); // Normal from Emrbee is already in object space.
        hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
        hit.primitiveID = embreeRayHit.hit.primID;

        const auto* pShape = pSceneObject->pShape.get();
        if (optLocalToWorldMatrix) {
            // Transform from world space to shape local space.
            Transform transform { *optLocalToWorldMatrix };
            Ray localRay = transform.transformToLocal(ray);
            // Hit is already in object space...
            //hit = transform.transformToLocal(hit);

            // Fill surface interaction in local space
            si = pShape->fillSurfaceInteraction(localRay, hit);

            // Transform surface interaction back to world space
            si = transform.transformToWorld(si);
        } else {
            // Tell surface interaction which the shape was hit.
            si = pShape->fillSurfaceInteraction(ray, hit);
        }

        // Flip the normal if it is facing away from the ray.
        if (glm::dot(si.normal, -ray.direction) < 0)
            si.normal = -si.normal;
        if (glm::dot(si.shading.normal, -ray.direction) < 0)
            si.shading.normal = -si.shading.normal;

        si.pSceneObject = pSceneObject;
        si.localToWorld = optLocalToWorldMatrix;
        si.shading.batchingPointColor = m_color;
        ray.tfar = embreeRayHit.ray.tfar;
        return true;
    } else {
        return false;
    }
}

template <typename HitRayState, typename AnyHitRayState>
bool BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAnyInternal(
    RTCScene scene, Ray& ray) const
{
    RTCRay embreeRay;
    embreeRay.org_x = ray.origin.x;
    embreeRay.org_y = ray.origin.y;
    embreeRay.org_z = ray.origin.z;
    embreeRay.dir_x = ray.direction.x;
    embreeRay.dir_y = ray.direction.y;
    embreeRay.dir_z = ray.direction.z;

    embreeRay.tnear = ray.tnear;
    embreeRay.tfar = ray.tfar;

    embreeRay.time = 0.0f;
    embreeRay.mask = 0xFFFFFFFF;
    embreeRay.id = 0;
    embreeRay.flags = 0;

    RTCIntersectContext context;
    rtcInitIntersectContext(&context);
    rtcOccluded1(scene, &context, &embreeRay);

    ray.tfar = embreeRay.tfar;

    static constexpr float minInf = -std::numeric_limits<float>::infinity();
    return embreeRay.tfar == minInf;
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState> BatchingAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    OPTICK_EVENT();
    spdlog::info("Creating batching points");

    using BatchingPointT = typename BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint;
    std::vector<BatchingPointT> batchingPoints;
    if (m_svdagRes > 0) {
        std::vector<std::optional<SparseVoxelDAG>> svdags;
        svdags.resize(m_subScenes.size());

        spdlog::info("Creating SVOs");

        // TODO: make cache thread safe and update this code...
        // Because the caches are not thread safe (yet) we have to load the data from the main thread..
        // We keep track of how many threads are working so that we never load new data faster than we can process it.
        const unsigned maxParallelism = std::max(1u, std::thread::hardware_concurrency() - 1);
        std::atomic_uint parallelTasks { 0 };

        tbb::task_group tg;
        for (size_t i = 0; i < m_subScenes.size(); i++) {
            while (parallelTasks.load(std::memory_order::memory_order_acquire) >= maxParallelism)
                continue;

            // Make resident sequentially
            const auto& subScene = m_subScenes[i];
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
            auto& subScene = m_subScenes[i];
            auto shapes = detail::getSubSceneShapes(subScene);
            batchingPoints.emplace_back(std::move(subScene), std::move(shapes), std::move(*svdags[i]), m_pGeometryCache, m_pTaskGraph);
        }
    } else {
        for (auto& subScene : m_subScenes) {
            auto shapes = detail::getSubSceneShapes(subScene);
            batchingPoints.emplace_back(std::move(subScene), std::move(shapes), m_pGeometryCache, m_pTaskGraph);
        }
    }

    spdlog::info("Constructing top level BVH over {} batching points", batchingPoints.size());

    // Moves batching points into internal structure
    PauseableBVH4<BatchingPointT, HitRayState, AnyHitRayState> topLevelBVH { batchingPoints };
    g_stats.scene.numBatchingPoints = batchingPoints.size();
    g_stats.memory.topBVH = topLevelBVH.sizeBytes();
    g_stats.memory.topBVHLeafs = batchingPoints.size() * sizeof(BatchingPointT);
    spdlog::info("PausableBVH constructed");
    return BatchingAccelerationStructure<HitRayState, AnyHitRayState>(
        m_embreeDevice, std::move(topLevelBVH), hitTask, missTask, anyHitTask, anyMissTask, m_pGeometryCache, m_pTaskGraph, m_botLevelBVHCacheSize);
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingAccelerationStructure(
    RTCDevice embreeDevice, PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>&& topLevelBVH,
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::LRUCacheTS* pGeometryCache, tasking::TaskGraph* pTaskGraph, size_t embreeSceneCacheSize)
    : m_embreeDevice(embreeDevice)
    , m_topLevelBVH(std::move(topLevelBVH))
    , m_embreeSceneCache(embreeSceneCacheSize)
    , m_pTaskGraph(pTaskGraph)
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
{
    for (auto& leaf : m_topLevelBVH.leafs())
        leaf.setParent(this, &m_embreeSceneCache);
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::~BatchingAccelerationStructure()
{
    rtcReleaseDevice(m_embreeDevice);
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
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
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
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