#pragma once
#include "pandora/graphics_core/pandora.h"
#include "stream/task_graph.h"
#include <embree3/rtcore.h>
#include <gsl/span>
#include <tuple>
#include <optional>

namespace pandora {

class EmbreeAccelerationStructureBuilder;

template <typename HitRayState, typename AnyHitRayState>
class EmbreeAccelerationStructure {
public:
    ~EmbreeAccelerationStructure();

    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

    std::optional<RayHit> intersectFast(const Ray& ray) const;

private:
    void intersectKernel(gsl::span<const std::tuple<Ray, HitRayState>> data, std::pmr::memory_resource* pMemoryResource);
    void intersectAnyKernel(gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource);

private:
    friend class EmbreeAccelerationStructureBuilder;
    EmbreeAccelerationStructure(
        RTCDevice embreeDevice, RTCScene embreeScene,
        tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
        tasking::TaskGraph* pTaskGraph);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;

    tasking::TaskGraph* m_pTaskGraph;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_intersectTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_intersectAnyTask;

    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

template <typename HitRayState, typename AnyHitRayState>
inline std::optional<RayHit> EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectFast(const Ray& ray) const
{
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
    rtcIntersect1(m_embreeScene, &context, &embreeRayHit);

    if (embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
        auto pSceneObject = reinterpret_cast<const SceneObject*>(rtcGetGeometryUserData(
            rtcGetGeometry(m_embreeScene, embreeRayHit.hit.geomID)));

        RayHit hit;
        hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
        hit.geometricNormal = glm::normalize(hit.geometricNormal);
        hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
        hit.sceneObjectRef = SceneObjectRef {
            nullptr, pSceneObject
        };
        hit.primitiveID = embreeRayHit.hit.primID;
        return hit;
    } else {
        return {};
    }
}

class EmbreeAccelerationStructureBuilder {
public:
    EmbreeAccelerationStructureBuilder(const Scene& scene, tasking::TaskGraph* pTaskGraph);

    template <typename HitRayState, typename AnyHitRayState>
    EmbreeAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;

    tasking::TaskGraph* m_pTaskGraph;
};

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState> EmbreeAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    return EmbreeAccelerationStructure(m_embreeDevice, m_embreeScene, hitTask, missTask, anyHitTask, anyMissTask, m_pTaskGraph);
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::EmbreeAccelerationStructure(
    RTCDevice embreeDevice, RTCScene embreeScene,
    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask,
    tasking::TaskGraph* pTaskGraph)
    : m_embreeDevice(embreeDevice)
    , m_embreeScene(embreeScene)
    , m_pTaskGraph(pTaskGraph)
    , m_intersectTask(
          pTaskGraph->addTask<std::tuple<Ray, HitRayState>>(
              [this](auto data, auto* pMemRes) { intersectKernel(data, pMemRes); }))
    , m_onHitTask(hitTask)
    , m_onMissTask(missTask)
    , m_onAnyHitTask(anyHitTask)
    , m_onAnyMissTask(anyMissTask)
{
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::~EmbreeAccelerationStructure()
{
    rtcReleaseScene(m_embreeScene);
    rtcReleaseDevice(m_embreeDevice);
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersect(const Ray& ray, const HitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectAny(const Ray& ray, const AnyHitRayState& state) const
{
    m_pTaskGraph->enqueue(m_intersectAnyTask, std::tuple { ray, state });
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectKernel(
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
            auto pSceneObject = reinterpret_cast<const SceneObject*>(rtcGetGeometryUserData(
                rtcGetGeometry(m_embreeScene, embreeRayHit.hit.geomID)));

            ray.tnear = embreeRayHit.ray.tnear;
            ray.tfar = embreeRayHit.ray.tfar;

            RayHit hit;
            hit.geometricNormal = { embreeRayHit.hit.Ng_x, embreeRayHit.hit.Ng_y, embreeRayHit.hit.Ng_z };
            hit.geometricNormal = glm::normalize(hit.geometricNormal);
            hit.geometricUV = { embreeRayHit.hit.u, embreeRayHit.hit.v };
            hit.sceneObjectRef = SceneObjectRef {
                nullptr, pSceneObject
            };
            hit.primitiveID = embreeRayHit.hit.primID;
            m_pTaskGraph->enqueue(m_onHitTask, std::tuple { ray, hit, state });
        } else {
            m_pTaskGraph->enqueue(m_onMissTask, std::tuple { ray, state });
        }
    }
}

template <typename HitRayState, typename AnyHitRayState>
inline void EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::intersectAnyKernel(
    gsl::span<const std::tuple<Ray, AnyHitRayState>> data, std::pmr::memory_resource* pMemoryResource)
{
}

}