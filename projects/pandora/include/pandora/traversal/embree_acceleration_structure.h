#pragma once
#include "pandora/graphics_core/pandora.h"
#include "stream/task_graph.h"
#include <embree3/rtcore.h>
#include <gsl/span>
#include <tuple>

namespace pandora {

class EmbreeAccelerationStructureBuilder;

template <typename HitRayState, typename AnyHitRayState>
class EmbreeAccelerationStructure {
public:
    ~EmbreeAccelerationStructure();

    void intersect(const Ray& ray, const HitRayState& state) const;
    void intersectAny(const Ray& ray, const AnyHitRayState& state) const;

private:
    friend class EmbreeAccelerationStructureBuilder;
    EmbreeAccelerationStructure(
        RTCDevice embreeDevice, RTCScene embreeScene,
        tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;

    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> m_onHitTask;
    tasking::TaskHandle<std::tuple<Ray, HitRayState>> m_onMissTask;
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> m_onAnyHitTask;
    tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> m_onAnyMissTask;
};

class EmbreeAccelerationStructureBuilder {
public:
    EmbreeAccelerationStructureBuilder(const Scene& scene);

    template <typename HitRayState, typename AnyHitRayState>
    EmbreeAccelerationStructure<HitRayState, AnyHitRayState> build(
        tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
        tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask);

private:
    RTCDevice m_embreeDevice;
    RTCScene m_embreeScene;
};

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState> EmbreeAccelerationStructureBuilder::build(
    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
{
    return EmbreeAccelerationStructure(m_embreeDevice, m_embreeScene, hitTask, missTask, anyHitTask, anyMissTask);
}

template <typename HitRayState, typename AnyHitRayState>
inline EmbreeAccelerationStructure<HitRayState, AnyHitRayState>::EmbreeAccelerationStructure(
    RTCDevice embreeDevice, RTCScene embreeScene,
    tasking::TaskHandle<std::tuple<Ray, RayHit, HitRayState>> hitTask, tasking::TaskHandle<std::tuple<Ray, HitRayState>> missTask,
    tasking::TaskHandle<std::tuple<Ray, RayHit, AnyHitRayState>> anyHitTask, tasking::TaskHandle<std::tuple<Ray, AnyHitRayState>> anyMissTask)
    : m_embreeDevice(embreeDevice)
    , m_embreeScene(embreeScene)
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

}