#include "pandora/integrators/direct_lighting_integrator.h"
#include "core/sampling.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

static thread_local MemoryArena s_memoryArena(4096);

DirectLightingIntegrator::DirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, LightStrategy strategy)
    : SamplerIntegrator(maxDepth, scene, sensor, spp)
    , m_strategy(strategy)
{
}

void DirectLightingIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& s, const EmbreeInsertHandle& h)
{
    s_memoryArena.reset();

    if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // TODO
        auto& sampler = getSampler(rayState.pixel);
        std::array samples = { sampler.get2D() };

        // Initialize common variables for Whitted integrator
        glm::vec3 n = si.shading.normal;
        glm::vec3 wo = si.wo;

        // Compute scattering functions for surface interaction
        si.computeScatteringFunctions(r, s_memoryArena);

        // Compute emitted light if ray hit an area light source
        Spectrum emitted = si.lightEmitted(wo);
        if (!isBlack(emitted)) {
            m_sensor.addPixelContribution(rayState.pixel, rayState.weight * emitted);
        }

        // Compute direct lighting for DirectLightingIntegrator
        if (m_strategy == LightStrategy::UniformSampleAll) {
            uniformSampleAllLights(rayState, si, sampler);
        } else if (m_strategy == LightStrategy::UniformSampleOne) {
            uniformSampleOneLight(rayState, si, sampler);
        } else {
            throw std::runtime_error("Unknown light strategy");
        }

        if (rayState.depth + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            specularReflect(si, sampler, s_memoryArena, rayState);
            specularTransmit(si, sampler, s_memoryArena, rayState);
        }
    } else if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);
        // Do nothing, in shadow

        if (rayState.addContributionOnLightHit) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            Spectrum li = si.lightEmitted(-r.direction);
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * li);
        }

        spawnNextSample(rayState.pixel);
    }
}

void DirectLightingIntegrator::rayMiss(const Ray& r, const RayState& s)
{
    if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);

        if (rayState.addContributionOnLightHit) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight * rayState.light->Le(-r.direction));
        } else {
            // Ray created by light sampling (PBRTv3 page 858) - contains radiance
            m_sensor.addPixelContribution(rayState.pixel, rayState.radianceOrWeight);
        }

        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // End of path: received radiance
        glm::vec3 radiance = glm::vec3(0.0f);

        auto lights = m_scene.getInfiniteLights();
        for (const auto& light : lights) {
            radiance += light->Le(r.direction);
        }

        assert(!std::isnan(glm::dot(rayState.weight, rayState.weight)) && glm::dot(rayState.weight, rayState.weight) > 0.0f);
        m_sensor.addPixelContribution(rayState.pixel, rayState.weight * radiance);

        spawnNextSample(rayState.pixel);
    }
}

// PBRTv3 page 854
void DirectLightingIntegrator::uniformSampleAllLights(const ContinuationRayState& r, const SurfaceInteraction& si, Sampler& sampler)
{
    for (const auto& light : m_scene.getLights()) {
        glm::vec2 uLight = sampler.get2D();
        glm::vec2 uScattering = sampler.get2D();
        estimateDirect(Spectrum(1.0f), r, si, uScattering, *light, uLight);
    }
}

void DirectLightingIntegrator::uniformSampleOneLight(const ContinuationRayState& r, const SurfaceInteraction& si, Sampler& sampler)
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
void DirectLightingIntegrator::estimateDirect(const Spectrum& multiplier, const ContinuationRayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular)
{
    BxDFType bsdfFlags = specular ? BSDF_ALL : BxDFType(BSDF_ALL | ~BSDF_SPECULAR);

    // Sample light source with multiple importance sampling
    auto lightSample = light.sampleLi(si, uLight);
    float scatteringPdf = 0.0f;
    float lightPdf = lightSample.pdf;
    if (lightSample.pdf > 0.0f && !lightSample.isBlack()) {
        // Compute BSDF value for light sample
        Spectrum f = si.bsdf->f(si.wo, lightSample.wi, bsdfFlags) * glm::abs(glm::dot(lightSample.wi, si.shading.normal));
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
