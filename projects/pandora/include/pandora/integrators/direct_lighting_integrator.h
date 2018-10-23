#pragma once
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

enum class LightStrategy { UniformSampleAll, UniformSampleOne };

class DirectLightingIntegrator : public SamplerIntegrator {
public:
	DirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, int parallelSamples = 1, LightStrategy strategy = LightStrategy::UniformSampleAll);
private:
    void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h) override final;
    void rayAnyHit(const Ray& r, const RayState& s) override final;
	void rayMiss(const Ray& r, const RayState& s) override final;

	void uniformSampleAllLights(const RayState& r, const SurfaceInteraction& si, Sampler& sampler);
	void uniformSampleOneLight(const RayState& r, const SurfaceInteraction& si, Sampler& sampler);

	void estimateDirect(float multiplier, const RayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular = false);
private:
	const LightStrategy m_strategy;
};

}
