#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "core/sampling.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"

namespace pandora {

static GrowingFreeListTS<ShadingMemoryArena::MemoryBlock> s_freeList;

NaiveDirectLightingIntegrator::NaiveDirectLightingIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, int parallelSamples)
    : SamplerIntegrator(maxDepth, scene, sensor, spp, parallelSamples)
{
}

void NaiveDirectLightingIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& rayState)
{
    ShadingMemoryArena memoryArena(s_freeList);

    if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& contRayState = std::get<ContinuationRayState>(rayState.data);

        // TODO
        std::array samples = { rayState.sampler->get2D() };

        // Initialize common variables for Whitted integrator
        glm::vec3 n = si.shading.normal;
        glm::vec3 wo = si.wo;

        // Compute scattering functions for surface interaction
        si.computeScatteringFunctions(r, memoryArena);

        // Compute emitted light if ray hit an area light source
        Spectrum emitted = si.Le(wo);
        if (!isBlack(emitted)) {
            m_sensor.addPixelContribution(rayState.pixel, contRayState.weight * emitted);
        }

        auto bsdfSample = si.bsdf->sampleF(wo, rayState.sampler->get2D());
        if (bsdfSample && !isBlack(bsdfSample->f)) {
            Spectrum radiance = bsdfSample->f * glm::abs(glm::dot(bsdfSample->wi, n)) / bsdfSample->pdf;
            Ray visibilityRay = si.spawnRay(bsdfSample->wi);
            spawnShadowRay(visibilityRay, false, rayState, radiance);
        }

        if (contRayState.bounces + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            specularReflect(si, *rayState.sampler, memoryArena, rayState);
            specularTransmit(si, *rayState.sampler, memoryArena, rayState);
        }
    } else if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);
        m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * si.Le(-r.direction));
    }
}

void NaiveDirectLightingIntegrator::rayMiss(const Ray& r, const RayState& rayState)
{
    if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);
        assert(!std::isnan(glm::dot(shadowRayState.radianceOrWeight, shadowRayState.radianceOrWeight)) && glm::dot(shadowRayState.radianceOrWeight, shadowRayState.radianceOrWeight) > 0.0f);

        for (const auto light : m_scene.getInfiniteLights())
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * light->Le(r));

        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& contRayState = std::get<ContinuationRayState>(rayState.data);

        // End of path: received radiance
        glm::vec3 radiance = glm::vec3(0.0f);

        auto lights = m_scene.getInfiniteLights();
        for (const auto& light : lights) {
            radiance += light->Le(r);
        }

        assert(!std::isnan(glm::dot(contRayState.weight, contRayState.weight)) && glm::dot(contRayState.weight, contRayState.weight) > 0.0f);
        m_sensor.addPixelContribution(rayState.pixel, contRayState.weight * radiance);

        spawnNextSample(rayState.pixel);
    }
}

}
