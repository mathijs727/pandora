#include "pandora/integrators/direct_lighting_integrator.h"
#include "core/sampling.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

static GrowingFreeListTS<ShadingMemoryArena::MemoryBlock> s_freeList;

DirectLightingIntegrator::DirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, LightStrategy strategy)
    : SamplerIntegrator(maxDepth, scene, sensor, spp)
    , m_strategy(strategy)
{
}

void DirectLightingIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& rayState, const InsertHandle& h)
{
    ShadingMemoryArena memoryArena(s_freeList);

    if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& contRayState = std::get<ContinuationRayState>(rayState.data);

        // Initialize common variables for Whitted integrator
        glm::vec3 wo = si.wo;

        // Compute scattering functions for surface interaction
        si.computeScatteringFunctions(r, memoryArena);

        // Compute emitted light if ray hit an area light source
        Spectrum emitted = si.Le(wo);
        if (!isBlack(emitted)) {
            m_sensor.addPixelContribution(rayState.pixel, contRayState.weight * emitted);
			spawnNextSample(rayState.pixel);
			return;
        }

        // Compute direct lighting for DirectLightingIntegrator
        if (m_strategy == LightStrategy::UniformSampleAll) {
            uniformSampleAllLights(rayState, si, *rayState.sampler);
        } else if (m_strategy == LightStrategy::UniformSampleOne) {
            uniformSampleOneLight(rayState, si, *rayState.sampler);
        } else {
            THROW_ERROR("Unknown light strategy");
        }

        if (contRayState.bounces + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            specularReflect(si, *rayState.sampler, memoryArena, rayState);
            specularTransmit(si, *rayState.sampler, memoryArena, rayState);
        }
    } else if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);

        assert(shadowRayState.light);
        if (si.sceneObjectMaterial->getPrimitiveAreaLight(si.primitiveID) == shadowRayState.light) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            Spectrum li = si.Le(-r.direction);
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * li);
        }
		
        spawnNextSample(rayState.pixel);
    }
}

void DirectLightingIntegrator::rayAnyHit(const Ray& r, const RayState& s)
{
    // Shadow ray spawned by light sampling => occluded so no contribution
#ifndef NDEBUG
    if (std::holds_alternative<ShadowRayState>(s.data)) {
        const auto& rayState = std::get<ShadowRayState>(s.data);
        assert(rayState.light == nullptr);
    }
#endif
}

void DirectLightingIntegrator::rayMiss(const Ray& r, const RayState& rayState)
{
    if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);

        if (shadowRayState.light != nullptr) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * shadowRayState.light->Le(r));
        } else {
            // Ray created by light sampling (PBRTv3 page 858) - contains radiance
			m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight);
        }

        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ContinuationRayState>(rayState.data);

        // End of path: received radiance
        glm::vec3 radiance = glm::vec3(0.0f);

        auto lights = m_scene.getInfiniteLights();
        for (const auto& light : lights) {
            radiance += light->Le(r);
        }

        assert(!std::isnan(glm::dot(shadowRayState.weight, shadowRayState.weight)) && glm::dot(shadowRayState.weight, shadowRayState.weight) > 0.0f);
        m_sensor.addPixelContribution(rayState.pixel, shadowRayState.weight * radiance);

        spawnNextSample(rayState.pixel);
    }
}

// PBRTv3 page 854
void DirectLightingIntegrator::uniformSampleAllLights(const RayState& rayState, const SurfaceInteraction& si, Sampler& sampler)
{
    for (const auto& light : m_scene.getLights()) {
        glm::vec2 uLight = sampler.get2D();
        glm::vec2 uScattering = sampler.get2D();
        estimateDirect(1.0f, rayState, si, uScattering, *light, uLight);
    }
}

void DirectLightingIntegrator::uniformSampleOneLight(const RayState& rayState, const SurfaceInteraction& si, Sampler& sampler)
{
    // Randomly choose a single light to sample
    int numLights = (int)m_scene.getLights().size();
    if (numLights == 0)
        return;
    int lightNum = std::min((int)(sampler.get1D() * numLights), numLights - 1);
    const auto& light = m_scene.getLights()[lightNum];

    glm::vec2 uLight = sampler.get2D();
    glm::vec2 uScattering = sampler.get2D();
    estimateDirect((float)numLights, rayState, si, uScattering, *light, uLight);
}

// PBRTv3 page 858
void DirectLightingIntegrator::estimateDirect(float multiplier, const RayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular)
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
