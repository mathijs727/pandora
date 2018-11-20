#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <atomic>
#include <tbb/scalable_allocator.h>
#include <tbb/flow_graph.h>
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
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);
    SamplerIntegrator(const SamplerIntegrator&) = delete;

    void reset() override final;
    void render(const PerspectiveCamera& camera) override final;

protected:
    using RayState = sampler_integrator::RayState;
    using ContinuationRayState = sampler_integrator::ContinuationRayState;
    using ShadowRayState = sampler_integrator::ShadowRayState;

    virtual void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s) override = 0;
    virtual void rayAnyHit(const Ray& r, const RayState& s) override = 0;
    virtual void rayMiss(const Ray& r, const RayState& s) override = 0;

    // Keep spawning samples until the camera ray does not miss all geometry. Usefull to prevent
    // stack overflow from the recursion caused by the miss shader (calling spawnNextSample) for 
    // pixels that do not cover any geometry.
    void spawnNextSampleTillSuccess();
    void spawnNextSample(bool initialRay = false);

    void specularReflect(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& rayState);
    void specularTransmit(const SurfaceInteraction& si, Sampler& sampler, ShadingMemoryArena& memoryArena, const RayState& rayState);

    void spawnShadowRay(const Ray& ray, bool anyHit, const RayState& s, const Spectrum& weight);
    void spawnShadowRay(const Ray& ray, bool anyHit, const RayState& s, const Spectrum& weight, const Light& light);

private:
    // Compute a Z-order (morton) curve over the screen tiles
    static std::vector<glm::ivec2> computeBlockStartLUT(const glm::ivec2& resolution);
    //glm::ivec2 indexToPixel(size_t pixelIndex) const;

protected:
    const int m_maxDepth;

private:
    PerspectiveCamera const* m_cameraThisFrame;
    tbb::scalable_allocator<UniformSampler> m_samplerAllocator;

    const glm::ivec2 m_resolution;
    const int m_maxSampleCount;

    static constexpr size_t s_tileSize = 8;
    static constexpr size_t s_pixelsInTile = s_tileSize * s_tileSize;
    const std::vector<glm::ivec2> m_blockStartLUT;

    //std::vector<std::atomic_int> m_pixelSampleCount;
    //const size_t m_maxPixelSample;
    std::atomic_size_t m_currentPixelSample;
};

}
