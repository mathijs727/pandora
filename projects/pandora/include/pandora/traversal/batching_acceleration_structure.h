#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/samplers/rng/pcg.h"
#include "pandora/traversal/acceleration_structure.h"
#include "pandora/traversal/pauseable_bvh/pauseable_bvh4.h"
#include "stream/cache/lru_cache.h"
#include "stream/task_graph.h"
#include <embree3/rtcore.h>
#include <glm/gtc/type_ptr.hpp>
#include <gsl/span>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pandora {

class BatchingAccelerationStructureBuilder;

struct SubScene {
    std::vector<std::pair<SceneNode*, std::optional<glm::mat4>>> sceneNodes;
    std::vector<SceneObject*> sceneObjects;
};

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
        tasking::TaskGraph* pTaskGraph);

    using TopLevelBVH = PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>;
    using OnHitTask = tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>>;
    using OnMissTask = tasking::TaskHandle<std::tuple<Ray, HitRayState>>;
    using OnAnyHitTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;
    using OnAnyMissTask = tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>>;

    class BatchingPoint {
    public:
        BatchingPoint(SubScene&& subScene, RTCScene embreeSubScene, tasking::TaskGraph* pTaskGraph);
        //BatchingPoint(BatchingPoint&&) noexcept = default;
        //~BatchingPoint();

        std::optional<bool> intersect(Ray&, SurfaceInteraction&, const HitRayState&, const PauseableBVHInsertHandle&) const;
        std::optional<bool> intersectAny(Ray&, const AnyHitRayState&, const PauseableBVHInsertHandle&) const;

        Bounds getBounds() const;
        bool intersectInternal(Ray&, SurfaceInteraction&) const;
        bool intersectAnyInternal(Ray&) const;

    protected:
        friend class BatchingAccelerationStructure<HitRayState, AnyHitRayState>;
        void setParent(BatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent);

    private:
        SubScene m_subScene;
        RTCScene m_embreeSubScene;
        glm::vec3 m_color;

        BatchingAccelerationStructure* m_pParent;
        tasking::TaskGraph* m_pTaskGraph;

        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> m_intersectTask;
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> m_intersectAnyTask;
    };

private:
    RTCDevice m_embreeDevice;
    //RTCScene m_rootEmbreeScene;
    TopLevelBVH m_topLevelBVH;

    tasking::TaskGraph* m_pTaskGraph;
    //tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_intersectTopLevelTask;
    //tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_intersectAnyTopLevelTask;

    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

class BatchingAccelerationStructureBuilder {
public:
    BatchingAccelerationStructureBuilder(
        Scene* pScene, stream::LRUCache* pCache, tasking::TaskGraph* pTaskGraph, unsigned primitivesPerBatchingPoint);

    template <typename HitRayState, typename AnyHitRayState>
    BatchingAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    void splitLargeSceneObjectsRecurse(SceneNode* pNode, stream::LRUCache* pCache, unsigned maxSize);
    static void verifyInstanceDepth(const SceneNode* pSceneNode, int depth = 0);
    static RTCScene buildEmbreeBVH(const SubScene& subScene, RTCDevice embreeDevice, std::unordered_map<const SceneNode*, RTCScene>& sceneCache);
    static RTCScene buildSubTreeEmbreeBVH(const SceneNode* pSceneNode, RTCDevice embreeDevice, std::unordered_map<const SceneNode*, RTCScene>& sceneCache);

private:
    RTCDevice m_embreeDevice;
    std::vector<SubScene> m_subScenes;

    tasking::TaskGraph* m_pTaskGraph;
};

inline glm::vec3 randomVec3()
{
    static PcgRng rng { 94892380 };
    return rng.uniformFloat3();
}

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    SubScene&& subScene, RTCScene embreeSubScene, tasking::TaskGraph* pTaskGraph)
    : m_subScene(std::move(subScene))
    , m_embreeSubScene(embreeSubScene)
    , m_color(randomVec3())
    , m_pTaskGraph(pTaskGraph)
{
}

/*template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(BatchingPoint&& other) noexcept
    : m_subScene(std::move(other.m_subScene))
    , m_embreeSubScene(other.m_embreeSubScene)
    , m_color(other.m_color)
    , m_pParent(other.m_pParent)
    , m_intersectTask(std::move(other.m_intersectTask))
    , m_intersectAnyTask(std::move(other.m_intersectAnyTask))
{
    other.m_embreeSubScene = nullptr;
}*/

/*template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::~BatchingPoint()
{
    if (m_embreeSubScene)
        rtcReleaseScene(m_embreeSubScene);
}*/

template <typename HitRayState, typename AnyHitRayState>
void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::setParent(
    BatchingAccelerationStructure<HitRayState, AnyHitRayState>* pParent)
{
    m_pParent = pParent;
    m_intersectTask = m_pTaskGraph->addTask<std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>>(
        [=](gsl::span<const std::tuple<Ray, SurfaceInteraction, HitRayState, PauseableBVHInsertHandle>> data, std::pmr::memory_resource* pMemoryResource) {
            std::vector<Ray> newRays;
            std::vector<SurfaceInteraction> newSurfaceInteractions;
            newRays.resize(data.size());
            newSurfaceInteractions.resize(data.size());

            int i = 0;
            for (const auto& [ray, si, state, insertHandle] : data) {
                auto& newRay = newRays[i];
                auto& newSi = newSurfaceInteractions[i++];
                newRay = ray;
                newSi = si;
                intersectInternal(newRay, newSi);
            }

            i = 0;
            for (const auto& [_, si, state, insertHandle] : data) {
                auto& newRay = newRays[i];
                auto& newSi = newSurfaceInteractions[i++];

                auto optHit = pParent->m_topLevelBVH.intersect(newRay, newSi, state, insertHandle);
                if (optHit) { // Ray exited BVH
                    assert(!optHit.value());

                    if (newSi.pSceneObject) {
                        // Ray hit something
                        assert(newSi.pSceneObject);
                        m_pTaskGraph->enqueue(pParent->m_onHitTask, std::tuple { newRay, newSi, state });
                    } else {
                        m_pTaskGraph->enqueue(pParent->m_onMissTask, std::tuple { newRay, state });
                    }
                }
            }
        });
    m_intersectAnyTask = m_pTaskGraph->addTask<std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>>(
        [=](gsl::span<const std::tuple<Ray, AnyHitRayState, PauseableBVHInsertHandle>> data, std::pmr::memory_resource* pMemoryResource) {
            std::vector<uint32_t> hits;
            hits.resize(data.size());

            int i = 0;
            for (const auto& [ray, state, insertHandle] : data) {
                auto mutRay = ray;
                hits[i++] = intersectAnyInternal(mutRay);
            }

            i = 0;
            for (const auto& [ray, state, insertHandle] : data) {
                bool hit = hits[i++];

                if (hit) {
                    m_pTaskGraph->enqueue(pParent->m_onAnyHitTask, std::tuple { ray, state });
                } else {
                    auto mutRay = ray;
                    auto optHit = pParent->m_topLevelBVH.intersectAny(mutRay, state, insertHandle);
                    if (optHit) { // Ray exited BVH
                        assert(!optHit.value());
                        m_pTaskGraph->enqueue(pParent->m_onAnyMissTask, std::tuple { ray, state });
                    } else {
                        // Nodes should always be paused, only way to return is when ray exists BVH
                        throw std::runtime_error("Invalid code path");
                    }
                }
            }
        });
}

template <typename HitRayState, typename AnyHitRayState>
Bounds BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::getBounds() const
{
    RTCBounds embreeBounds;
    rtcGetSceneBounds(m_embreeSubScene, &embreeBounds);
    return Bounds(embreeBounds);
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
    m_pTaskGraph->enqueue(m_intersectTask, std::tuple { ray, si, userState, bvhInsertHandle });
    return {};
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAny(
    Ray& ray, const AnyHitRayState& userState, const PauseableBVHInsertHandle& bvhInsertHandle) const
{
    m_pTaskGraph->enqueue(m_intersectAnyTask, std::tuple { ray, userState, bvhInsertHandle });
    return {};
}

template <typename HitRayState, typename AnyHitRayState>
bool BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectInternal(
    Ray& ray, SurfaceInteraction& si) const
{
    // TODO: batching
    RTCIntersectContext context {};

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
    rtcIntersect1(m_embreeSubScene, &context, &embreeRayHit);

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        std::optional<glm::mat4> transform;
        const SceneObject* pSceneObject { nullptr };

        if (embreeRayHit.hit.instID[0] == 0) {
            pSceneObject = reinterpret_cast<const SceneObject*>(
                rtcGetGeometryUserData(rtcGetGeometry(m_embreeSubScene, embreeRayHit.hit.geomID)));
        } else {
            glm::mat4 accumulatedTransform { 1.0f };
            RTCScene scene = m_embreeSubScene;
            for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
                unsigned geomID = embreeRayHit.hit.instID[i];
                if (geomID == 0)
                    break;

                RTCGeometry geometry = rtcGetGeometry(scene, geomID);

                glm::mat4 localTransform;
                rtcGetGeometryTransform(geometry, 0.0f, RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, glm::value_ptr(localTransform));
                accumulatedTransform *= localTransform;

                scene = reinterpret_cast<RTCScene>(rtcGetGeometryUserData(geometry));
            }

            transform = accumulatedTransform;
            pSceneObject = reinterpret_cast<const SceneObject*>(
                rtcGetGeometryUserData(rtcGetGeometry(scene, embreeRayHit.hit.geomID)));
        }

        RayHit hit;
        hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
        hit.geometricNormal = glm::normalize(glm::dot(-ray.direction, hit.geometricNormal) > 0.0f ? hit.geometricNormal : -hit.geometricNormal);
        hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
        hit.primitiveID = embreeRayHit.hit.primID;

        ray.tfar = embreeRayHit.ray.tfar;

        const auto* pShape = pSceneObject->pShape.get();
        si = pShape->fillSurfaceInteraction(ray, hit);
        si.pSceneObject = pSceneObject;
        si.localToWorld = transform;
        si.shading.batchingPointColor = m_color;
        return true;
    } else {
        return false;
    }
}

template <typename HitRayState, typename AnyHitRayState>
bool BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAnyInternal(Ray& ray) const
{
    // TODO: batching
    RTCIntersectContext context {};

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

    rtcOccluded1(m_embreeSubScene, &context, &embreeRay);

    ray.tfar = embreeRay.tfar;
    static constexpr float minInf = -std::numeric_limits<float>::infinity();
    return embreeRay.tfar != minInf;
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState> BatchingAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    spdlog::info("Creating batching points");

    std::unordered_map<const SceneNode*, RTCScene> embreeSceneCache;

    using BatchingPointT = typename BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint;
    std::vector<BatchingPointT> batchingPoints;
    std::transform(std::begin(m_subScenes), std::end(m_subScenes), std::back_inserter(batchingPoints),
        [&](SubScene& subScene) {
            //verifyInstanceDepth(pSubSceneRoot.get());

            std::unordered_map<const SceneNode*, RTCScene> sceneCache;
            RTCScene embreeSubScene = buildEmbreeBVH(subScene, m_embreeDevice, embreeSceneCache);

            return BatchingPointT { std::move(subScene), embreeSubScene, m_pTaskGraph };
        });

    spdlog::info("Constructing top level BVH");

    // Moves batching points into internal structure
    PauseableBVH4<BatchingPointT, HitRayState, AnyHitRayState> topLevelBVH { batchingPoints };
    return BatchingAccelerationStructure<HitRayState, AnyHitRayState>(m_embreeDevice, std::move(topLevelBVH), hitTask, missTask, anyHitTask, anyMissTask, m_pTaskGraph);
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingAccelerationStructure(
    RTCDevice embreeDevice, PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState>&& topLevelBVH,
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::TaskGraph* pTaskGraph)
    : m_embreeDevice(embreeDevice)
    , m_topLevelBVH(std::move(topLevelBVH))
    , m_pTaskGraph(pTaskGraph)
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
{
    for (auto& leaf : m_topLevelBVH.leafs())
        leaf.setParent(this);
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::~BatchingAccelerationStructure()
{
    rtcReleaseDevice(m_embreeDevice);
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
{
    auto mutRay = ray;
    SurfaceInteraction si;
    auto optHit = m_topLevelBVH.intersect(mutRay, si, state);
    if (optHit) {
        assert(!optHit.value());
        m_pTaskGraph->enqueue(m_onMissTask, std::tuple { mutRay, state });
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
{
    auto mutRay = ray;
    auto optHit = m_topLevelBVH.intersectAny(mutRay, state);
    if (optHit) {
        assert(!optHit.value());
        m_pTaskGraph->enqueue(m_onAnyMissTask, std::tuple { mutRay, state });
    }
}

}