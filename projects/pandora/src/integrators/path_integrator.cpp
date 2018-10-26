#include "pandora/integrators/path_integrator.h"
#include "core/sampling.h"
#include "pandora/core/pandora.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/free_list_backed_memory_arena.h"
#include "pandora/utility/growing_free_list_ts.h"
#include "pandora/utility/math.h"
#include "pandora/utility/memory_arena.h"
#include <gsl/gsl>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

static thread_local GrowingFreeListTS<ShadingMemoryArena::MemoryBlock> s_freeList;

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp, int parallelSamples)
    : SamplerIntegrator(maxDepth, scene, sensor, spp, parallelSamples)
{
}

// PBRTv3 page 877
void PathIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& rayState)
{
    ShadingMemoryArena memoryArena(s_freeList);

    if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& contRayState = std::get<ContinuationRayState>(rayState.data);

        // Possibly add emitted light at intersection
        if (contRayState.bounces == 0 || contRayState.specularBounce) {
            // Add emitted light at path vertex or environment
            m_sensor.addPixelContribution(rayState.pixel, si.Le(-r.direction));
        }

        // Compute scattering functions
        si.computeScatteringFunctions(r, memoryArena, pandora::Radiance, true);

        //TODO: skip over medium boundaries (volume rendering)

        // Terminate path if maxDepth was reached
        if (contRayState.bounces >= m_maxDepth) {
            spawnNextSample(rayState.pixel);
            return;
        }

        // Sample illumination from lights to find path contribution
        uniformSampleOneLight(rayState, si, *rayState.sampler);

        // Sample BSDF to get new path direction
        glm::vec3 wo = -r.direction;
        if (auto bsdfSampleOpt = si.bsdf->sampleF(wo, rayState.sampler->get2D(), BSDF_ALL); bsdfSampleOpt && !isBlack(bsdfSampleOpt->f) && bsdfSampleOpt->pdf > 0.0f) {
            const auto& bsdfSample = *bsdfSampleOpt;

            ContinuationRayState newContRayState = contRayState;
            newContRayState.weight *= bsdfSample.f * absDot(bsdfSample.wi, si.shading.normal) / bsdfSample.pdf;
            newContRayState.bounces++;
            newContRayState.specularBounce = (bsdfSample.sampledType & BSDF_SPECULAR) != 0;
            Ray newRay = si.spawnRay(bsdfSample.wi);

            // Possibly terminate the path with Russian roulette
            if (newContRayState.bounces > 3) {
                float q = std::max(0.05f, 1 - newContRayState.weight.y);
                if (rayState.sampler->get1D() < q) {
                    spawnNextSample(rayState.pixel);
                    return;
                }
                newContRayState.weight /= 1.0f - q;
            }

            RayState newRayState = {
                newContRayState,
                rayState.pixel,
                rayState.sampler
            };
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&newRay, 1), gsl::make_span(&newRayState, 1));
        } else {
            spawnNextSample(rayState.pixel);
            return;
        }

        //TODO: Account for subsurface scattering, if applicable

    } else if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);

        assert(shadowRayState.light);
        if (si.sceneObjectMaterial->getPrimitiveAreaLight(si.primitiveID) == shadowRayState.light) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            Spectrum li = si.Le(-r.direction);
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * li);
        }
    }
}

void PathIntegrator::rayAnyHit(const Ray& r, const RayState& s)
{
    // Shadow ray spawned by light sampling => occluded so no contribution
#ifndef NDEBUG
    if (std::holds_alternative<ShadowRayState>(s.data)) {
        const auto& rayState = std::get<ShadowRayState>(s.data);
        assert(rayState.light == nullptr);
    }
#endif
}

void PathIntegrator::rayMiss(const Ray& r, const RayState& rayState)
{
    if (std::holds_alternative<ContinuationRayState>(rayState.data)) {
        const auto& contRayState = std::get<ContinuationRayState>(rayState.data);

        // Possibly add emitted light at intersection
        if (contRayState.bounces == 0 || contRayState.specularBounce) {
            // Add emitted light at path vertex or environment
            for (const auto& light : m_scene.getInfiniteLights())
                m_sensor.addPixelContribution(rayState.pixel, contRayState.weight * light->Le(r));
        }

        // Terminate path if ray escaped
        spawnNextSample(rayState.pixel);
    } else if (std::holds_alternative<ShadowRayState>(rayState.data)) {
        const auto& shadowRayState = std::get<ShadowRayState>(rayState.data);

        if (shadowRayState.light != nullptr) {
            // Ray created by BSDF sampling (PBRTv3 page 861) - contains weight
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight * shadowRayState.light->Le(r));
        } else {
            // Ray created by light sampling (PBRTv3 page 858) - contains radiance
            m_sensor.addPixelContribution(rayState.pixel, shadowRayState.radianceOrWeight);
        }
    }
}

void PathIntegrator::uniformSampleOneLight(const RayState& rayState, const SurfaceInteraction& si, Sampler& sampler)
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
void PathIntegrator::estimateDirect(float multiplier, const RayState& rayState, const SurfaceInteraction& si, const glm::vec2& uScattering, const Light& light, const glm::vec2& uLight, bool specular)
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
                    spawnShadowRay(lightSample.visibilityRay, true, rayState, multiplier * f * lightSample.radiance / lightPdf);
                } else {
                    float weight = powerHeuristic(1, lightPdf, 1, scatteringPdf);
                    spawnShadowRay(lightSample.visibilityRay, true, rayState, multiplier * f * lightSample.radiance * weight / lightPdf);
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
                spawnShadowRay(ray, false, rayState, multiplier * f * weight / scatteringPdf, light);
            }
        }
    }
}

}
