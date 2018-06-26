#include "pandora/integrators/path_integrator.h"
#include "core/sampling.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"
#include <gsl/gsl>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

static thread_local MemoryArena s_memoryArena(4096);

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp)
    : SamplerIntegrator(maxDepth, scene, sensor, spp)
{
}

// PBRTv3 page 877
void PathIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const EmbreeInsertHandle& h)
{
    s_memoryArena.reset();

    if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // TODO
        auto& sampler = getSampler(rayState.pixel);

        // Possibly add emitted light at intersection
        if (rayState.bounces == 0 || rayState.specularBounce) {
            // Add emitted light at path vertex or environment
            m_sensor.addPixelContribution(rayState.pixel, si.Le(-r.direction));
        }

        // Compute scattering functions
        si.computeScatteringFunctions(r, s_memoryArena, pandora::Radiance, true);

        //TODO: skip over medium boundaries (volume rendering)

        // Terminate path if maxDepth was reached
        if (rayState.bounces >= m_maxDepth) {
            spawnNextSample(rayState.pixel);
            return;
        }

        // Sample illumination from lights to find path contribution
        uniformSampleOneLight(rayState, si, sampler);

        // Sample BSDF to get new path direction
        glm::vec3 wo = -r.direction;
		if (auto bsdfSampleOpt = si.bsdf->sampleF(wo, sampler.get2D(), BSDF_ALL); bsdfSampleOpt && !isBlack(bsdfSampleOpt->f) && bsdfSampleOpt->pdf > 0.0f) {
			const auto& bsdfSample = *bsdfSampleOpt;
			
			ContinuationRayState newRayState = rayState;
			newRayState.weight *= bsdfSample.f * absDot(bsdfSample.wi, si.shading.normal) / bsdfSample.pdf;
			newRayState.bounces++;
			newRayState.specularBounce = (bsdfSample.sampledType & BSDF_SPECULAR) != 0;
			Ray newRay = si.spawnRay(bsdfSample.wi);

			// Possibly terminate the path with Russian roulette
			if (newRayState.bounces > 3)
			{
				float q = std::max(0.05f, 1 - newRayState.weight.y);
				if (sampler.get1D() < q)
				{
					spawnNextSample(rayState.pixel);
					return;
				}
				newRayState.weight /= 1.0f - q;
			}

			RayState rayStateVariant = newRayState;
			m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&newRay, 1));
        } else {
            spawnNextSample(rayState.pixel);
            return;
        }

        //TODO: Account for subsurface scattering, if applicable


    } else if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);

        if (rayState.light != nullptr && si.sceneObject->getAreaLight(si.primitiveID) == rayState.light) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            Spectrum li = si.Le(-r.direction);
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * li);
        }
    }
}

void PathIntegrator::rayMiss(const Ray& r, const RayState& s)
{
    if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // Possibly add emitted light at intersection
        if (rayState.bounces == 0 || rayState.specularBounce) {
            // Add emitted light at path vertex or environment
            for (const auto& light : m_scene.getInfiniteLights())
                m_sensor.addPixelContribution(rayState.pixel, rayState.weight * light->Le(r));
        }

        // Terminate path if ray escaped
        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);

        if (rayState.light != nullptr) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * rayState.light->Le(r));
        } else {
            // Ray created by light sampling (PBRTv3 page 858) - contains radiance
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight);
        }
    }
}

void PathIntegrator::uniformSampleOneLight(const ContinuationRayState& r, const SurfaceInteraction& si, Sampler& sampler)
{
    // Randomly choose a single light to sample
    int numLights = (int)m_scene.getLights().size();
    if (numLights == 0)
        return;
    int lightNum = std::min((int)(sampler.get1D() * numLights), numLights - 1);
    const auto& light = m_scene.getLights()[lightNum];

    glm::vec2 uLight = sampler.get2D();
    glm::vec2 uScattering = sampler.get2D();
    estimateDirect(Spectrum(numLights), r, si, uScattering, *light, uLight);
}

// PBRTv3 page 858
void PathIntegrator::estimateDirect(const Spectrum& multiplier, const ContinuationRayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular)
{
    BxDFType bsdfFlags = specular ? BSDF_ALL : BxDFType(BSDF_ALL | ~BSDF_SPECULAR);

    // Sample light source with multiple importance sampling
    auto lightSample = light.sampleLi(si, uLight);
    float scatteringPdf = 0.0f;
    float lightPdf = lightSample.pdf;

    if (lightPdf > 0.0f && !lightSample.isBlack()) {
        // Compute BSDF value for light sample
        Spectrum f = si.bsdf->f(si.wo, lightSample.wi, bsdfFlags) * absDot(lightSample.wi, si.shading.normal);
        scatteringPdf = si.bsdf->pdf(si.wo, lightSample.wi, bsdfFlags);

        if (!isBlack(f)) {
            // Add light's contribution to reflected radiance
            if (!isBlack(lightSample.radiance)) {
                if (light.isDeltaLight()) {
                    spawnShadowRay(lightSample.visibilityRay, rayState, multiplier * f * lightSample.radiance / lightPdf);
                } else {
                    float weight = powerHeuristic(1, lightPdf, 1, scatteringPdf);
                    spawnShadowRay(lightSample.visibilityRay, rayState, multiplier * f * lightSample.radiance * weight / lightPdf);
                }
            }
        }
    }

    // Sample BSDF with multiple importance sampling
    // ...
    if (!light.isDeltaLight()) {
        // Sample scattered direction for surface interaction
        if (auto bsdfSampleOpt = si.bsdf->sampleF(si.wo, uScattering, bsdfFlags); bsdfSampleOpt) {
            auto bsdfSample = *bsdfSampleOpt;
            float scatteringPdf = bsdfSample.pdf;

            Spectrum f = bsdfSample.f * absDot(bsdfSample.wi, si.shading.normal);
            bool sampledSpecular = bsdfSample.sampledType & BSDF_SPECULAR;

            if (!isBlack(f) && scatteringPdf > 0.0f) {
                // Account for light contribution along sampled direction wi
                float weight = 1.0f;
                if (!sampledSpecular) {
                    lightPdf = light.pdfLi(si, bsdfSample.wi);
                    if (lightPdf == 0.0f)
                        return;

                    weight = powerHeuristic(1, scatteringPdf, 1, lightPdf);
                }

                // Find intersection and compute transmittance
                Ray ray = si.spawnRay(bsdfSample.wi);
                spawnShadowRay(ray, rayState, multiplier * f * weight / scatteringPdf, light);
            }
        }
    }
}

}
