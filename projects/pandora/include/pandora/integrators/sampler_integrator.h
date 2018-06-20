#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <variant>

namespace pandora {

namespace sampler_integrator {
    struct ContinuationRayState {
        glm::ivec2 pixel;
        glm::vec3 weight;
        int depth;
    };
    struct ShadowRayState {
        glm::ivec2 pixel;
        glm::vec3 contribution;
    };
    using RayState = std::variant<ContinuationRayState, ShadowRayState>;
}

class SamplerIntegrator : public Integrator<sampler_integrator::RayState> {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);

    void render(const PerspectiveCamera& camera) final;

protected:
    void rayHit(const Ray& r, const SurfaceInteraction& si, const sampler_integrator::RayState& s, const EmbreeInsertHandle& h) final;
    void rayMiss(const Ray& r, const sampler_integrator::RayState& s) final;

private:
    void spawnNextSample(const glm::vec2& pixel, bool initialSample = false);

    void specularReflect(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const sampler_integrator::ContinuationRayState& rayState);
    void specularTransmit(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const sampler_integrator::ContinuationRayState& rayState);

    void spawnShadowRay(const Ray& ray, const sampler_integrator::ContinuationRayState& s, const Spectrum& multiplier);

private:
    const int m_maxDepth;

    PerspectiveCamera const* m_cameraThisFrame;
};

}
