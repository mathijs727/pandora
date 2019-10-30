#pragma once
#include "pandora/graphics_core/bounds.h"
#include "pandora/graphics_core/pandora.h"
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

template <typename HitRayState, typename AnyHitRayState>
class BatchingAccelerationStructure {
public:
    ~BatchingAccelerationStructure();

    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

private:
    void intersectKernel(gsl::span<const std::tuple<Ray, HitRayState>> data, std::pmr::memory_resource* pMemoryResource);
    void intersectAnyKernel(gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource);

private:
    friend class BatchingAccelerationStructureBuilder;
    BatchingAccelerationStructure(
        RTCDevice embreeDevice, RTCScene embreeScene,
        tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
        tasking::TaskGraph* pTaskGraph);

    class BatchingPoint {
    public:
        BatchingPoint(RTCScene embreeSubScene, const std::shared_ptr<SceneNode>& pSubSceneRoot);
        BatchingPoint(BatchingPoint&&);
        ~BatchingPoint();

        Bounds getBounds() const;
        std::optional<bool> intersect(Ray&, RayHit&, const HitRayState&);
        std::optional<bool> intersectAny(Ray&, const AnyHitRayState&);

    private:
        RTCScene embreeSubScene;
        std::shared_ptr<SceneNode> pSubSceneRoot;
    };

private:
    RTCDevice m_embreeDevice;
    //RTCScene m_rootEmbreeScene;
    PauseableBVH4<BatchingPoint, HitRayState, AnyHitRayState> m_topLevelBVH;

    tasking::TaskGraph* m_pTaskGraph;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_intersectTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_intersectAnyTask;

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

private:
    Scene* m_pScene;
    RTCDevice m_embreeDevice;

    std::vector<std::shared_ptr<SceneNode>> m_subScenes;

    tasking::TaskGraph* m_pTaskGraph;
};

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(
    RTCScene embreeSubScene, const std::shared_ptr<SceneNode>& pSubSceneRoot)
    : embreeSubScene(embreeSubScene)
    , pSubSceneRoot(pSubSceneRoot)
{
}

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::BatchingPoint(BatchingPoint&& other)
    : embreeSubScene(other.embreeSubScene)
    , pSubSceneRoot(std::move(other.pSubSceneRoot))
{
    other.embreeSubScene = nullptr;
}

template <typename HitRayState, typename AnyHitRayState>
BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::~BatchingPoint()
{
    if (embreeSubScene)
        rtcReleaseScene(embreeSubScene);
}
template <typename HitRayState, typename AnyHitRayState>
Bounds BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::getBounds() const
{
    RTCBounds embreeBounds;
    rtcGetSceneBounds(m_rootEmbreeScene, &embreeBounds);
    return Bounds(embreeBounds)
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersect(
    Ray& ray, RayHit& rayHit, const HitRayState& userState)
{
    // TODO: batching
}

template <typename HitRayState, typename AnyHitRayState>
std::optional<bool> BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingPoint::intersectAny(
    Ray& ray, const AnyHitRayState& userState)
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

    rtcOccluded1(m_embreeScene, &context, &embreeRay);

    constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
    if (embreeRay.tfar == negativeInfinity) {
        m_pTaskGraph->enqueue(m_onAnyHitTask, std::tuple { ray, state });
    } else {
        m_pTaskGraph->enqueue(m_onAnyMissTask, std::tuple { ray, state });
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState> BatchingAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    return BatchingAccelerationStructure(m_embreeDevice, m_embreeScene, hitTask, missTask, anyHitTask, anyMissTask, m_pTaskGraph);
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::BatchingAccelerationStructure(
    RTCDevice embreeDevice, RTCScene embreeScene,
    tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::TaskGraph* pTaskGraph)
    : m_embreeDevice(embreeDevice)
    , m_embreeScene(embreeScene)
    , m_pTaskGraph(pTaskGraph)
    , m_intersectTask(
          pTaskGraph->addTask<std::tuple<Ray, HitRayState>>(
              [this](auto data, auto* pMemRes) { intersectKernel(data, pMemRes); }))
    , m_intersectAnyTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyHitRayState>>(
              [this](auto data, auto* pMemRes) { intersectAnyKernel(data, pMemRes); }))
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
{
}

template <typename HitRayState, typename AnyHitRayState>
inline BatchingAccelerationStructure<HitRayState, AnyHitRayState>::~BatchingAccelerationStructure()
{
    rtcReleaseScene(m_embreeScene);
    rtcReleaseDevice(m_embreeDevice);
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectAnyTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectKernel(
    gsl::span<const std::tuple<Ray, HitRayState>> data, std::pmr::memory_resource* pMemoryResource)
{
    RTCIntersectContext context {};
    for (auto [ray, state] : data) {
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
        rtcIntersect1(m_embreeScene, &context, &embreeRayHit);

        if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            std::optional<glm::mat4> transform;
            const SceneObject* pSceneObject { nullptr };

            if (embreeRayHit.hit.instID[0] == 0) {
                pSceneObject = reinterpret_cast<const SceneObject*>(
                    rtcGetGeometryUserData(rtcGetGeometry(m_embreeScene, embreeRayHit.hit.geomID)));
            } else {
                glm::mat4 accumulatedTransform { 1.0f };
                RTCScene scene = m_embreeScene;
                for (int i = 0; i < RTC_MAX_INSTANCE_LEVEL_COUNT; i++) {
                    unsigned geomID = embreeRayHit.hit.instID[i];
                    if (geomID == RTC_INVALID_GEOMETRY_ID)
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

            const auto* pShape = pSceneObject->pShape.get();
            auto si = pShape->fillSurfaceInteraction(ray, hit);
            si.pSceneObject = pSceneObject;
            si.localToWorld = transform;
            m_pTaskGraph->enqueue(m_onHitTask, std::tuple { ray, si, state });
        } else {
            m_pTaskGraph->enqueue(m_onMissTask, std::tuple { ray, state });
        }
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline void BatchingAccelerationStructure<HitRayState, AnyHitRayState>::intersectAnyKernel(
    gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource)
{
    RTCIntersectContext context {};
    for (auto [ray, state] : data) {
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

        rtcOccluded1(m_embreeScene, &context, &embreeRay);

        constexpr float negativeInfinity = -std::numeric_limits<float>::infinity();
        if (embreeRay.tfar == negativeInfinity) {
            m_pTaskGraph->enqueue(m_onAnyHitTask, std::tuple { ray, state });
        } else {
            m_pTaskGraph->enqueue(m_onAnyMissTask, std::tuple { ray, state });
        }
    }
}
}