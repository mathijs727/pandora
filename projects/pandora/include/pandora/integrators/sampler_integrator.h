#pragma once
#include "pandora/core/integrator.h"
#include "pandora/core/pandora.h"
#include <variant>

namespace pandora {

namespace sampler_integrator {
    struct ContinuationRayState {
        glm::ivec2 pixel;
        glm::vec3 weight;
        int bounces;
		bool specularBounce;
    };
    struct ShadowRayState {
        glm::ivec2 pixel;
        glm::vec3 radianceOrWeight;
		
		// Used for importance sampling (PBRTv3 page 861)
		const Light* light;// If ray misses then we should only add contribution from the (infinite) light that we tried to sample (and not all infinite lights)
    };
    using RayState = std::variant<ContinuationRayState, ShadowRayState>;
}

class SamplerIntegrator : public Integrator<sampler_integrator::RayState> {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);

    void render(const PerspectiveCamera& camera) final;

    bool m_firstFrame;
    std::vector<uint32_t> m_previousHitPrimitiveID;
protected:
	using RayState = sampler_integrator::RayState;
	using ContinuationRayState = sampler_integrator::ContinuationRayState;
	using ShadowRayState = sampler_integrator::ShadowRayState;
    virtual void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) = 0;
    virtual void rayMiss(const Ray& r, const RayState& s) = 0;

    void spawnNextSample(const glm::ivec2& pixel, bool initialSample = false);

    void specularReflect(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const sampler_integrator::ContinuationRayState& rayState);
    void specularTransmit(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const sampler_integrator::ContinuationRayState& rayState);

	void spawnShadowRay(const Ray& ray, const sampler_integrator::ContinuationRayState& s, const Spectrum& weight);
	void spawnShadowRay(const Ray& ray, const sampler_integrator::ContinuationRayState& s, const Spectrum& weight, const Light& light);

protected:
    const int m_maxDepth;
private:
    PerspectiveCamera const* m_cameraThisFrame;
};

}
