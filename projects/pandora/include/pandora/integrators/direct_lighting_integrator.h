#pragma once
#include "pandora/integrators/sampler_integrator.h"

namespace pandora {

enum class LightStrategy { UniformSampleAll, UniformSampleOne };

class DirectLightingIntegrator : public SamplerIntegrator {
public:
	DirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, LightStrategy strategy);
private:
	void rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const EmbreeInsertHandle& h) override final;
	void rayMiss(const Ray& r, const RayState& s) override final;

	void uniformSampleAllLights(const ContinuationRayState& r, const SurfaceInteraction& si, Sampler& sampler);
	void uniformSampleOneLight(const ContinuationRayState& r, const SurfaceInteraction& si, Sampler& sampler);

	void estimateDirect(const Spectrum& multiplier, const ContinuationRayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular = false);
private:
	const LightStrategy m_strategy;
};

}
