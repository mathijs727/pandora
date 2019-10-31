#pragma once
#include "pandora/graphics_core/pandora.h"
#include "pandora/samplers/rng/pcg.h"
#include "pandora/traversal/embree_acceleration_structure.h"
#include <atomic>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <tuple>

namespace pandora {

enum class LightStrategy {
    UniformSampleAll,
    UniformSampleOne
};

class SamplerIntegrator {
public:
    SamplerIntegrator(tasking::TaskGraph* pTaskGraph, int maxDepth, int spp, LightStrategy strategy = LightStrategy::UniformSampleAll);

    struct BounceRayState {
        glm::ivec2 pixel { 0 };
        glm::vec3 weight { 0 };
        int pathDepth { 0 };
        PcgRng rng;
    };
    struct ShadowRayState {
        glm::ivec2 pixel;
        glm::vec3 radiance;
    };
    using RayState = BounceRayState;
    using AnyRayState = ShadowRayState;

    using HitTaskHandle = tasking::TaskHandle<std::tuple<Ray, SurfaceInteraction, RayState>>;
    using MissTaskHandle = tasking::TaskHandle<std::tuple<Ray, RayState>>;
    using AnyHitTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;
    using AnyMissTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;

    using Accel = EmbreeAccelerationStructure<RayState, AnyRayState>;
    void render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel, size_t seed = 891379);

protected:
    void spawnNewPaths(int numPaths);

    void uniformSampleAllLights(const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng);
    void uniformSampleOneLight(const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng);

    void estimateDirect(
        const SurfaceInteraction& si,
        const Light& light,
        float weight,
        const BounceRayState& bounceRayState,
        PcgRng& rng);

private:
    void spawnShadowRay(const Ray& shadowRay, PcgRng& rng, const BounceRayState& bounceRayState, const Spectrum& radiance);

protected:
    tasking::TaskGraph* m_pTaskGraph;

    const int m_maxDepth;
    const int m_maxSpp;
    const LightStrategy m_strategy;

    // TODO: make render state local to render() instead of spreading it around the class
    struct RenderData {
        const PerspectiveCamera* pCamera;
        Sensor* pSensor;
        std::atomic_int currentRayIndex;
        size_t seed;
        glm::ivec2 resolution;
        glm::vec2 fResolution;
        int maxPixelIndex;

        const Scene* pScene;
        const Accel* pAccelerationStructure;
    };
    std::unique_ptr<RenderData> m_pCurrentRenderData;
};

}
