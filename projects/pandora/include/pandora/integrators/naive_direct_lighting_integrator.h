#pragma once
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

class DirectLightingIntegrator : public SamplerIntegrator {
public:
    DirectLightingIntegrator(tasking::TaskGraph* pTaskGraph, tasking::LRUCache* pGeomCache, int maxDepth, int spp, LightStrategy strategy = LightStrategy::UniformSampleAll);

    HitTaskHandle hitTaskHandle() const;
    MissTaskHandle missTaskHandle() const;
    AnyHitTaskHandle anyHitTaskHandle() const;
    AnyMissTaskHandle anyMissTaskHandle() const;

private:
    void rayHit(const Ray& ray, const SurfaceInteraction& si, RayState state, MemoryArena& memoryArena);
    void rayMiss(const Ray& ray, const RayState& state);
    void rayAnyHit(const Ray& ray, const AnyRayState& state);
    void rayAnyMiss(const Ray& ray, const AnyRayState& state);

    void specularReflect(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena);
    void specularTransmit(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena);

private:
    HitTaskHandle m_hitTask;
    MissTaskHandle m_missTask;
    AnyHitTaskHandle m_anyHitTask;
    AnyMissTaskHandle m_anyMissTask;
};

}
