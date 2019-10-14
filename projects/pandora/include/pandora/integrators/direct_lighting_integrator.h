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

class DirectLightingIntegrator {
public:
    DirectLightingIntegrator(tasking::TaskGraph* pTaskGraph, int maxDepth, int spp, LightStrategy strategy = LightStrategy::UniformSampleAll);

    struct BounceRayState {
        glm::ivec2 pixel;
        glm::vec3 weight;
        int pathDepth;
        PcgRng rng;
    };
    struct ShadowRayState {
        glm::ivec2 pixel;
        glm::vec3 radiance;
    };
    using RayState = BounceRayState;
    using AnyRayState = ShadowRayState;

    using HitTaskHandle = tasking::TaskHandle<std::tuple<Ray, RayHit, RayState>>;
    using MissTaskHandle = tasking::TaskHandle<std::tuple<Ray, RayState>>;
    using AnyHitTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;
    using AnyMissTaskHandle = tasking::TaskHandle<std::tuple<Ray, AnyRayState>>;

    HitTaskHandle hitTaskHandle() const;
    MissTaskHandle missTaskHandle() const;
    AnyHitTaskHandle anyHitTaskHandle() const;
    AnyMissTaskHandle anyMissTaskHandle() const;

    using Accel = EmbreeAccelerationStructure<RayState, AnyRayState>;
    void render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel);

private:
    void rayHit(const Ray& ray, const SurfaceInteraction& si, RayState state, MemoryArena& memoryArena);
    void rayMiss(const Ray& ray, const RayState& state);
    void rayAnyHit(const Ray& ray, const AnyRayState& state);
    void rayAnyMiss(const Ray& ray, const AnyRayState& state);

    void spawnNewPaths(int numPaths);
    /*void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s) override final;
    void rayAnyHit(const Ray& r, const RayState& s) override final;
    void rayMiss(const Ray& r, const RayState& s) override final;*/

    void uniformSampleAllLights(const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng);
    void uniformSampleOneLight(const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng);

    void estimateDirect(
        const SurfaceInteraction& si,
        const Light& light,
        float weight,
        const BounceRayState& bounceRayState,
        PcgRng& rng);

    void spawnShadowRay(const Ray& shadowRay, PcgRng& rng, const BounceRayState& bounceRayState, const Spectrum& radiance);

    void specularReflect(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena);
    void specularTransmit(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena);

private:
    tasking::TaskGraph* m_pTaskGraph;

    HitTaskHandle m_hitTask;
    MissTaskHandle m_missTask;
    AnyHitTaskHandle m_anyHitTask;
    AnyMissTaskHandle m_anyMissTask;

    const int m_maxDepth;
    const int m_maxSpp;
    const LightStrategy m_strategy;

    // TODO: make render state local to render() instead of spreading it around the class
    struct RenderData {
        const PerspectiveCamera* pCamera;
        Sensor* pSensor;
        std::atomic_int currentRayIndex;
        glm::ivec2 resolution;
        int maxPixelIndex;

        const Scene* pScene;
        const Accel* pAccelerationStructure;
    };
    std::unique_ptr<RenderData> m_pCurrentRenderData;
};

}
