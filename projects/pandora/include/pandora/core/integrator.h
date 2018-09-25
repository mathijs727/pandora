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

    void startNewFrame();
    virtual void render(const PerspectiveCamera& camera) = 0;

    int getCurrentFrameSpp() const;

protected:
    using InsertHandle = typename InCoreAccelerationStructure<IntegratorState>::InsertHandle;
    virtual void rayHit(const Ray& r, SurfaceInteraction si, const IntegratorState& s, const InsertHandle& h) = 0;
    virtual void rayAnyHit(const Ray& r, const IntegratorState& s) = 0;
    virtual void rayMiss(const Ray& r, const IntegratorState& s) = 0;

    void resetSamplers();
    Sampler& getSampler(const glm::ivec2& pixel);

protected:
    const Scene& m_scene;
    //InCoreAccelerationStructure<IntegratorState> m_accelerationStructure;
    //InCoreBatchingAccelerationStructure<IntegratorState> m_accelerationStructure;
    OOCBatchingAccelerationStructure<IntegratorState> m_accelerationStructure;

    Sensor& m_sensor;
    const int m_sppPerCall;
    int m_sppThisFrame;

private:
    std::vector<UniformSampler> m_samplers;
};

template <typename IntegratorState>
inline Integrator<IntegratorState>::Integrator(const Scene& scene, Sensor& sensor, int sppPerCall)
    : m_scene(scene)
    , m_accelerationStructure(
          1024llu * 1024llu * 25, // Geometry memory limit
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
    , m_sppPerCall(sppPerCall)
    , m_sppThisFrame(0)
{
    // The following would only call the constructor once and create copies (which means each sampler generates the same random numbers)
    // m_samplers(sensor.getResolution().x * sensor.getResolution().y, spp)

    int pixelCount = sensor.getResolution().x * sensor.getResolution().y;
    m_samplers.reserve(pixelCount);
    std::mt19937 randomEngine;
    std::uniform_int_distribution<unsigned> samplerSeedDistribution;
    for (int i = 0; i < pixelCount; i++)
        m_samplers.push_back(UniformSampler(sppPerCall, samplerSeedDistribution(randomEngine)));
}

template <typename IntegratorState>
inline Integrator<IntegratorState>::~Integrator()
{
    g_stats.asyncTriggerSnapshot();
}

template <typename IntegratorState>
inline void Integrator<IntegratorState>::startNewFrame()
{
    m_sppThisFrame = 0;
    m_sensor.clear(glm::vec3(0.0f));
}

template <typename IntegratorState>
inline int Integrator<IntegratorState>::getCurrentFrameSpp() const
{
    return m_sppThisFrame;
}

template <typename IntegratorState>
inline void Integrator<IntegratorState>::resetSamplers()
{
    for (auto& sampler : m_samplers)
        sampler.reset();
}

template <typename IntegratorState>
inline Sampler& Integrator<IntegratorState>::getSampler(const glm::ivec2& pixel)
{
    return m_samplers[pixel.y * m_sensor.getResolution().x + pixel.x];
}

}
