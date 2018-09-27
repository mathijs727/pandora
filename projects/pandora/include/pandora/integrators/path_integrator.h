#pragma once
#include "pandora/core/pandora.h"
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

class PathIntegrator : public SamplerIntegrator {
public:
    // WARNING: do not modify the scene in any way while the integrator is alive
    PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp);

protected:
    void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) override final;
    void rayAnyHit(const Ray& r, const RayState& s) override final;
    void rayMiss(const Ray& r, const RayState& s) override final;

	// Copy-pasta from DirectLightingIntegrator but I don't want to extent from it since the ray hit / miss handlers make assumptions based on the rays spawned by these functions
	void uniformSampleOneLight(const RayState& r, const SurfaceInteraction& si, Sampler& sampler);
	void estimateDirect(float multiplier, const RayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular = false);
};

}
