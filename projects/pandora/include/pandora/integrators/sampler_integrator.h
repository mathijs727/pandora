#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <atomic>
#include <tbb/scalable_allocator.h>
#include <variant>

namespace pandora {

namespace sampler_integrator {
    struct ContinuationRayState {
        glm::vec3 weight;
        int bounces;
        bool specularBounce;
    };
    struct ShadowRayState {
        glm::ivec2 pixel;
        glm::vec3 radianceOrWeight;

        // Used for importance sampling (PBRTv3 page 861)
        const Light* light; // If ray misses then we should only add contribution from the (infinite) light that we tried to sample (and not all infinite lights)
    };

    struct RayState {
        std::variant<ContinuationRayState, ShadowRayState> data;

        glm::ivec2 pixel;
        std::shared_ptr<UniformSampler> sampler;
    };

}

class SamplerIntegrator : public Integrator<sampler_integrator::RayState> {
    static constexpr bool RANDOM_SEEDS = false;
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, int parallelSamples = 1);
    SamplerIntegrator(const SamplerIntegrator&) = delete;

    void reset() override final;
    void render(const PerspectiveCamera& camera) override final;

protected:
    using RayState = sampler_integrator::RayState;
    using ContinuationRayState = sampler_integrator::ContinuationRayState;
    using ShadowRayState = sampler_integrator::ShadowRayState;

    virtual void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) override = 0;
    virtual void rayAnyHit(const Ray& r, const RayState& s) override = 0;
    virtual void rayMiss(const Ray& r, const RayState& s) override = 0;

    void spawnNextSample(const glm::ivec2& pixel);

    void specularReflect(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& rayState);
    void specularTransmit(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& rayState);

    void spawnShadowRay(const Ray& ray, bool anyHit, const RayState& s, const Spectrum& weight);
    void spawnShadowRay(const Ray& ray, bool anyHit, const RayState& s, const Spectrum& weight, const Light& light);

private:
    int pixelToIndex(const glm::ivec2& pixel) const;

protected:
    const int m_maxDepth;

private:
    PerspectiveCamera const* m_cameraThisFrame;
    tbb::scalable_allocator<UniformSampler> m_samplerAllocator;

    const glm::ivec2 m_resolution;
    std::vector<std::atomic_int> m_pixelSampleCount;
    const int m_maxSampleCount;
    const int m_parallelSamples;
};

}
