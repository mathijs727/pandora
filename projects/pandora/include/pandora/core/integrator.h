#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/scene.h"
#include "pandora/core/sensor.h"
#include "pandora/samplers/uniform_sampler.h"
#include "pandora/traversal/in_core_acceleration_structure.h"
#include "pandora/traversal/in_core_batching_acceleration_structure.h"
#include "pandora/traversal/ooc_batching_acceleration_structure.h"
#include <random>

namespace pandora {

template <typename IntegratorState>
class Integrator {
public:
    Integrator(const Scene& scene, Sensor& sensor, int sppPerCall);
    virtual ~Integrator();

    virtual void reset() = 0;
    virtual void render(const PerspectiveCamera& camera) = 0;

protected:
    using InsertHandle = typename InCoreAccelerationStructure<IntegratorState>::InsertHandle;
    virtual void rayHit(const Ray& r, SurfaceInteraction si, const IntegratorState& s, const InsertHandle& h) = 0;
    virtual void rayAnyHit(const Ray& r, const IntegratorState& s) = 0;
    virtual void rayMiss(const Ray& r, const IntegratorState& s) = 0;

protected:
    const Scene& m_scene;
    //InCoreAccelerationStructure<IntegratorState> m_accelerationStructure;
    //InCoreBatchingAccelerationStructure<IntegratorState> m_accelerationStructure;
    OOCBatchingAccelerationStructure<IntegratorState> m_accelerationStructure;

    Sensor& m_sensor;
};

template <typename IntegratorState>
inline Integrator<IntegratorState>::Integrator(const Scene& scene, Sensor& sensor, int sppPerCall)
    : m_scene(scene)
    , m_accelerationStructure(
          1024llu * 1024llu * 50, // Geometry memory limit
          scene,
          //scene.getInCoreSceneObjects(),
          [this](const Ray& r, const SurfaceInteraction& si, const IntegratorState& s, const InsertHandle& h) {
              rayHit(r, si, s, h);
          },
          [this](const Ray& r, const IntegratorState& s) {
              rayAnyHit(r, s);
          },
          [this](const Ray& r, const IntegratorState& s) {
              rayMiss(r, s);
          })
    , m_sensor(sensor)
{
}

template <typename IntegratorState>
inline Integrator<IntegratorState>::~Integrator()
{
    g_stats.asyncTriggerSnapshot();
}

}
