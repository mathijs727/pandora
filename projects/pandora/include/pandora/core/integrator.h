#pragma once
#include "pandora/core/pandora.h"
#include "pandora/core/scene.h"
#include "pandora/samplers/uniform_sampler.h"
#include "pandora/traversal/embree_accel.h"

namespace pandora {

template <typename IntegratorState>
class Integrator {
public:
    Integrator(const Scene& scene, Sensor& sensor, int spp);

    void resetSamplers();
    virtual void render(const PerspectiveCamera& camera) = 0;

protected:
    virtual void rayHit(const Ray& r, const SurfaceInteraction& si, const IntegratorState& s, const EmbreeInsertHandle& h) = 0;
    virtual void rayMiss(const Ray& r, const IntegratorState& s) = 0;

    Sampler& getSampler(const glm::ivec2& pixel);

protected:
    const Scene& m_scene;
    EmbreeAccel<IntegratorState> m_accelerationStructure;

    Sensor& m_sensor;
    int m_spp;

private:
    std::vector<UniformSampler> m_samplers;
};

template<typename IntegratorState>
inline void Integrator<IntegratorState>::resetSamplers()
{
    for (auto& sampler : m_samplers)
        sampler.reset();
}

template <typename IntegratorState>
inline Integrator<IntegratorState>::Integrator(const Scene& scene, Sensor& sensor, int spp)
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
    //, m_samplers(sensor.getResolution().x * sensor.getResolution().y, spp)
    , m_spp(spp)
{
    // The following would only call the constructor once and create copies (which means each sampler generates the same random numbers)
    // m_samplers(sensor.getResolution().x * sensor.getResolution().y, spp)

    int pixelCount = sensor.getResolution().x * sensor.getResolution().y;
    m_samplers.reserve(pixelCount);
    for (int i = 0 ; i< pixelCount; i++)
        m_samplers.push_back(UniformSampler(spp));
}

template <typename IntegratorState>
inline Sampler& Integrator<IntegratorState>::getSampler(const glm::ivec2& pixel)
{
    return m_samplers[pixel.y * m_sensor.getResolution().x + pixel.x];
}

}
