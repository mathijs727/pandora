#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/scene.h"
#include "pandora/samplers/uniform_sampler.h"
#include "pandora/traversal/embree_accel.h"

namespace pandora {

template <typename IntegratorState>
class Integrator {
public:
    Integrator(const Scene& scene, Sensor& sensor);

    virtual void render(const PerspectiveCamera& camera) = 0;

protected:
    virtual void rayHit(const Ray& r, const SurfaceInteraction& si, const IntegratorState& s, const EmbreeInsertHandle& h) = 0;
    virtual void rayMiss(const Ray& r, const IntegratorState& s) = 0;

    Sampler& getSampler(const glm::ivec2& pixel);

protected:
    const Scene& m_scene;
    EmbreeAccel<IntegratorState> m_accelerationStructure;

    Sensor& m_sensor;

private:
    std::vector<UniformSampler> m_samplers;
};

template <typename IntegratorState>
inline Integrator<IntegratorState>::Integrator(const Scene& scene, Sensor& sensor)
    : m_scene(scene)
    , m_accelerationStructure(
          scene.getSceneObjects(),
          [this](const Ray& r, const SurfaceInteraction& si, const IntegratorState& s, const EmbreeInsertHandle& h) {
              rayHit(r, si, s, h);
          },
          [this](const Ray& r, const IntegratorState& s) {
              rayMiss(r, s);
          })
    , m_sensor(sensor)
    , m_samplers(sensor.getResolution().x * sensor.getResolution().y, 4)
{
}

template <typename IntegratorState>
inline Sampler& Integrator<IntegratorState>::getSampler(const glm::ivec2& pixel)
{
    return m_samplers[pixel.y * m_sensor.getResolution().x + pixel.x];
}

}
