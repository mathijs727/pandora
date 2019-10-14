#include "pandora/integrators/direct_lighting_integrator.h"
#include "graphics_core/sampling.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/interaction.h"
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/perspective_camera.h"
#include "pandora/graphics_core/ray.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/utility/math.h"
#include <spdlog/spdlog.h>

namespace pandora {

DirectLightingIntegrator::DirectLightingIntegrator(
    tasking::TaskGraph* pTaskGraph, int maxDepth, int spp, LightStrategy strategy)
    : m_pTaskGraph(pTaskGraph)
    , m_hitTask(
          pTaskGraph->addTask<std::tuple<Ray, RayHit, RayState>>(
              [this](gsl::span<const std::tuple<Ray, RayHit, RayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, rayHit, state] : hits) {
                      auto pShadingGeometry = rayHit.pSceneObject->pShape->getShadingGeometry();
                      auto pMaterial = rayHit.pSceneObject->pMaterial;

                      // TODO: make this use pMemoryResource
                      MemoryArena memoryArena;
                      auto si = pShadingGeometry->fillSurfaceInteraction(ray, rayHit);
                      pMaterial->computeScatteringFunctions(si, memoryArena);
                      si.pSceneObject = rayHit.pSceneObject;
                      this->rayHit(ray, si, state, memoryArena);
                  }
              }))
    , m_missTask(
          pTaskGraph->addTask<std::tuple<Ray, RayState>>(
              [this](gsl::span<const std::tuple<Ray, RayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      this->rayMiss(ray, state);
                  }
              }))
    , m_anyHitTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : hits) {
                      this->rayAnyHit(ray, state);
                  }
              }))
    , m_anyMissTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      this->rayAnyMiss(ray, state);
                  }
              }))
    , m_maxDepth(maxDepth)
    , m_maxSpp(spp)
    , m_strategy(strategy)
{
}

void DirectLightingIntegrator::render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel)
{
    auto resolution = sensor.getResolution();

    auto pRenderData = std::make_unique<RenderData>();
    pRenderData->pCamera = &camera;
    pRenderData->pSensor = &sensor;
    pRenderData->currentRayIndex.store(0);
    pRenderData->resolution = resolution;
    pRenderData->maxPixelIndex = resolution.x * resolution.y;
    pRenderData->pScene = &scene;
    pRenderData->pAccelerationStructure = &accel;
    m_pCurrentRenderData = std::move(pRenderData);

    // Spawn initial rays
    spawnNewPaths(1000);
    m_pTaskGraph->run();
}

void DirectLightingIntegrator::rayHit(const Ray& ray, const SurfaceInteraction& si, BounceRayState state, MemoryArena& memoryArena)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    PcgRng rng = state.rng;

    // Next Event Estimation (NEE) samples light sources so random bounce should ignore it.
    if (si.pSceneObject->pAreaLight || state.pathDepth > m_maxDepth) {
        spawnNewPaths(1);
        return;
    }
    /*// Compute emitted light if ray hit an area light source
    Spectrum emitted = si.Le(si.wo);
    if (!isBlack(emitted))
        pSensor->addPixelContribution(state.pixel, state.weight * emitted);*/

    // Sample direct light using Next Event Estimation (NEE)
    if (m_strategy == LightStrategy::UniformSampleAll)
        uniformSampleAllLights(si, state, rng);
    else
        uniformSampleOneLight(si, state, rng);

    // Possibly terminate the path with Russian roulette
    if (state.pathDepth > 3) {
        float q = std::max(0.05f, 1 - state.weight.y);
        if (rng.uniformFloat() < q) {
            spawnNewPaths(1);
            return;
        }
        state.weight /= 1.0f - q;
    }

    // Random bounce
    if (!randomBounce(si, state, rng, memoryArena)) {
        spawnNewPaths(1);
        return;
    }
}

void DirectLightingIntegrator::rayMiss(const Ray& ray, const BounceRayState& state)
{
    spawnNewPaths(1);
}

void DirectLightingIntegrator::rayAnyHit(const Ray& ray, const ShadowRayState& state)
{
}

void DirectLightingIntegrator::rayAnyMiss(const Ray& ray, const ShadowRayState& state)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    pSensor->addPixelContribution(state.pixel, state.radiance);
}

void DirectLightingIntegrator::uniformSampleAllLights(
    const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng)
{
    const auto* pScene = m_pCurrentRenderData->pScene;
    for (const auto& areaLightInstance : pScene->areaLightInstances) {
        // TODO: support instanced lights
        assert(!areaLightInstance.transform);
        estimateDirect(si, *areaLightInstance.pAreaLight, 1.0f, bounceRayState, rng);
    }
}

void DirectLightingIntegrator::uniformSampleOneLight(
    const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng)
{
    const auto* pScene = m_pCurrentRenderData->pScene;

    // Randomly choose a single light to sample
    uint32_t numLights = static_cast<uint32_t>(pScene->areaLightInstances.size());
    if (numLights == 0)
        return;

    uint32_t lightNum = std::min(rng.uniformU32(), numLights - 1);
    const auto& areaLightInstance = pScene->areaLightInstances[lightNum];
    // TODO: support instanced lights
    assert(!areaLightInstance.transform);

    estimateDirect(si, *areaLightInstance.pAreaLight, static_cast<float>(numLights), bounceRayState, rng);
}

void DirectLightingIntegrator::estimateDirect(
    const SurfaceInteraction& si,
    const Light& light,
    float multiplier,
    const BounceRayState& bounceRayState,
    PcgRng& rng)
{
    //BxDFType bsdfFlags = specular ? BSDF_ALL : BxDFType(BSDF_ALL | ~BSDF_SPECULAR);
    BxDFType bsdfFlags = BxDFType(BSDF_ALL | ~BSDF_SPECULAR);

    // Sample light source with multiple importance sampling
    auto lightSample = light.sampleLi(si, rng);

    if (lightSample.pdf > 0.0f && !lightSample.isBlack()) {
        // Compute BSDF value for light sample
        Spectrum f = si.pBSDF->f(si.wo, lightSample.wi, bsdfFlags) * absDot(lightSample.wi, si.shading.normal);

        if (!isBlack(f) && !isBlack(lightSample.radiance)) {
            spawnShadowRay(lightSample.visibilityRay, rng, bounceRayState, multiplier * f * lightSample.radiance / lightSample.pdf);
        }
    }
}

void DirectLightingIntegrator::spawnShadowRay(const Ray& shadowRay, PcgRng& rng, const BounceRayState& bounceRayState, const Spectrum& radiance)
{
    ShadowRayState shadowRayState;
    shadowRayState.pixel = bounceRayState.pixel;
    shadowRayState.radiance = bounceRayState.weight * radiance;
    m_pCurrentRenderData->pAccelerationStructure->intersectAny(shadowRay, shadowRayState);
}

bool DirectLightingIntegrator::randomBounce(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena)
{
    // Sample BSDF to get new path direction
    if (auto bsdfSampleOpt = si.pBSDF->sampleF(si.wo, rng.uniformFloat2(), BSDF_ALL); bsdfSampleOpt && !isBlack(bsdfSampleOpt->f) && bsdfSampleOpt->pdf > 0.0f) {
        const auto& bsdfSample = *bsdfSampleOpt;

        Ray ray = si.spawnRay(bsdfSample.wi);

        BounceRayState rayState;
        rayState.pathDepth = prevRayState.pathDepth + 1;
        rayState.pixel = prevRayState.pixel;
        rayState.rng = PcgRng(rng.uniformU64());
        rayState.weight = prevRayState.weight * bsdfSample.f * absDot(bsdfSample.wi, si.shading.normal) / bsdfSample.pdf;

        m_pCurrentRenderData->pAccelerationStructure->intersect(ray, rayState);
        return true;
    } else {
        return false;
    }
}

/*void DirectLightingIntegrator::specularReflect(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena)
{
    // Compute specular reflection wi and BSDF value
    BxDFType type = BxDFType(BSDF_REFLECTION | BSDF_SPECULAR);
    auto sample = si.pBSDF->sampleF(si.wo, glm::vec2 { rng.uniformFloat(), rng.uniformFloat() }, type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    glm::vec3 ns = si.shading.normal;
    if (sample->pdf > 0.0f && isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        Ray ray = si.spawnRay(sample->wi);

        BounceRayState rayState;
        rayState.pathDepth = prevRayState.pathDepth + 1;
        rayState.pixel = prevRayState.pixel;
        rayState.rng = PcgRng(rng.uniformU64());
        rayState.weight = prevRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        m_pCurrentRenderData->pAccelerationStructure->intersect(ray, rayState);
    }
}

void DirectLightingIntegrator::specularTransmit(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena)
{
    // Compute specular reflection wi and BSDF value
    BxDFType type = BxDFType(BSDF_TRANSMISSION);
    auto sample = si.pBSDF->sampleF(si.wo, glm::vec2 { rng.uniformFloat(), rng.uniformFloat() }, type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    glm::vec3 ns = si.shading.normal;
    if (sample->pdf > 0.0f && isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        Ray ray = si.spawnRay(sample->wi);

        BounceRayState rayState;
        rayState.pathDepth = prevRayState.pathDepth + 1;
        rayState.pixel = prevRayState.pixel;
        rayState.rng = PcgRng(rng.uniformU64());
        rayState.weight = prevRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        m_pCurrentRenderData->pAccelerationStructure->intersect(ray, rayState);
    }
}*/

void DirectLightingIntegrator::spawnNewPaths(int numPaths)
{
    auto* pRenderData = m_pCurrentRenderData.get();
    const int startIndex = pRenderData->currentRayIndex.fetch_add(numPaths);
    const int endIndex = std::min(startIndex + numPaths, pRenderData->maxPixelIndex * m_maxSpp);

    for (int i = startIndex; i < endIndex; i++) {
        //const int spp = i % m_maxSpp;
        const int pixelIndex = i / m_maxSpp;
        const int x = pixelIndex % pRenderData->resolution.x;
        const int y = pixelIndex / pRenderData->resolution.x;

        BounceRayState rayState;
        rayState.pixel = glm::ivec2 { x, y };
        rayState.weight = glm::vec3(1.0f);
        rayState.pathDepth = 0;
        rayState.rng = PcgRng(i);

        CameraSample cameraSample;
        cameraSample.pixel = glm::vec2(x, y) + rayState.rng.uniformFloat2();

        const Ray cameraRay = pRenderData->pCamera->generateRay(cameraSample);
        pRenderData->pAccelerationStructure->intersect(cameraRay, rayState);
    }
}

DirectLightingIntegrator::HitTaskHandle DirectLightingIntegrator::hitTaskHandle() const
{
    return m_hitTask;
}

DirectLightingIntegrator::MissTaskHandle DirectLightingIntegrator::missTaskHandle() const
{
    return m_missTask;
}

DirectLightingIntegrator::AnyHitTaskHandle DirectLightingIntegrator::anyHitTaskHandle() const
{
    return m_anyHitTask;
}

DirectLightingIntegrator::AnyMissTaskHandle DirectLightingIntegrator::anyMissTaskHandle() const
{
    return m_anyMissTask;
}

/*void DirectLightingIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const RayState& rayState)
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
            spawnNextSample();
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

        /*if (contRayState.bounces + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            specularReflect(si, *rayState.sampler, memoryArena, rayState);
            specularTransmit(si, *rayState.sampler, memoryArena, rayState);
        }/
        spawnNextSample();
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

        spawnNextSample();
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
}*/
}
