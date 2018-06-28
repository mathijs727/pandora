#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "core/sampling.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

static thread_local MemoryArena s_memoryArena(4096);

NaiveDirectLightingIntegrator::NaiveDirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp)
    : SamplerIntegrator(maxDepth, scene, sensor, spp)
{
}

void NaiveDirectLightingIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const InsertHandle& h)
{
    s_memoryArena.reset();

    if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

		m_sensor.addPixelContribution(rayState.pixel, Spectrum(glm::abs(si.shading.normal)));
		return;

        // TODO
        auto& sampler = getSampler(rayState.pixel);
        std::array samples = { sampler.get2D() };

        // Initialize common variables for Whitted integrator
        glm::vec3 n = si.shading.normal;
        glm::vec3 wo = si.wo;

        // Compute scattering functions for surface interaction
        si.computeScatteringFunctions(r, s_memoryArena);

        // Compute emitted light if ray hit an area light source
        Spectrum emitted = si.Le(wo);
        if (!isBlack(emitted)) {
            m_sensor.addPixelContribution(rayState.pixel, rayState.weight * emitted);
        }

		auto bsdfSample = si.bsdf->sampleF(wo, sampler.get2D());
		if (bsdfSample)
		{
			Spectrum f = si.bsdf->f(wo, bsdfSample->wi);
			if (!isBlack(f)) {
				Spectrum radiance = f * glm::abs(glm::dot(bsdfSample->wi, n)) / bsdfSample->pdf;
				Ray visibilityRay = si.spawnRay(bsdfSample->wi);
				spawnShadowRay(visibilityRay, rayState, radiance);
			}
		}

        if (rayState.bounces + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            specularReflect(si, sampler, s_memoryArena, rayState);
            specularTransmit(si, sampler, s_memoryArena, rayState);
        }
    } else if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);
        // Do nothing, in shadow
		m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * si.Le(-r.direction));
    }
}

void NaiveDirectLightingIntegrator::rayMiss(const Ray& r, const RayState& s)
{
    if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);
        assert(!std::isnan(glm::dot(rayState.radianceOrWeight, rayState.radianceOrWeight)) && glm::dot(rayState.radianceOrWeight, rayState.radianceOrWeight) > 0.0f);
		for (const auto light : m_scene.getInfiniteLights())
			m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * light->Le(r));

        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // End of path: received radiance
        glm::vec3 radiance = glm::vec3(0.0f);

        auto lights = m_scene.getInfiniteLights();
        for (const auto& light : lights) {
            radiance += light->Le(r);
        }

        assert(!std::isnan(glm::dot(rayState.weight, rayState.weight)) && glm::dot(rayState.weight, rayState.weight) > 0.0f);
        m_sensor.addPixelContribution(rayState.pixel, rayState.weight * radiance);

        spawnNextSample(rayState.pixel);
    }
}

}
