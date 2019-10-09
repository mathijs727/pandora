#pragma once
#include "pandora/config.h"
#include "pandora/graphics_core/pandora.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/sensor.h"
//#include "pandora/eviction/fifo_cache.h"
#include "pandora/samplers/uniform_sampler.h"
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
    virtual void rayHit(const Ray& r, SurfaceInteraction si, const IntegratorState& s) = 0;
    virtual void rayAnyHit(const Ray& r, const IntegratorState& s) = 0;
    virtual void rayMiss(const Ray& r, const IntegratorState& s) = 0;

protected:
    const Scene& m_scene;
    //AccelerationStructure<IntegratorState> m_accelerationStructure;

    Sensor& m_sensor;
};


}
