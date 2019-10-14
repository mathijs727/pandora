#include "pandora/integrators/path_integrator.h"
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

PathIntegrator::PathIntegrator(
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

void PathIntegrator::render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel)
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

void PathIntegrator::rayHit(const Ray& ray, const SurfaceInteraction& si, BounceRayState state, MemoryArena& memoryArena)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    PcgRng rng = state.rng;

    // Next Event Estimation (NEE) samples light sources so random bounce should ignore light source hits (without Multiple Importance Sampling).
    if (si.pSceneObject->pAreaLight || state.pathDepth > m_maxDepth) {
        spawnNewPaths(1);
        return;
    }

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

    // Spawn random bounce
    if (!randomBounce(si, state, rng, memoryArena)) {
        spawnNewPaths(1);
        return;
    }
}

void PathIntegrator::rayMiss(const Ray& ray, const BounceRayState& state)
{
    spawnNewPaths(1);
}

void PathIntegrator::rayAnyHit(const Ray& ray, const ShadowRayState& state)
{
}

void PathIntegrator::rayAnyMiss(const Ray& ray, const ShadowRayState& state)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    pSensor->addPixelContribution(state.pixel, state.radiance);
}

void PathIntegrator::uniformSampleAllLights(
    const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng)
{
    const auto* pScene = m_pCurrentRenderData->pScene;
    for (const auto& areaLightInstance : pScene->areaLightInstances) {
        // TODO: support instanced lights
        assert(!areaLightInstance.transform);
        estimateDirect(si, *areaLightInstance.pAreaLight, 1.0f, bounceRayState, rng);
    }
}

void PathIntegrator::uniformSampleOneLight(
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

void PathIntegrator::estimateDirect(
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

void PathIntegrator::spawnShadowRay(const Ray& shadowRay, PcgRng& rng, const BounceRayState& bounceRayState, const Spectrum& radiance)
{
    ShadowRayState shadowRayState;
    shadowRayState.pixel = bounceRayState.pixel;
    shadowRayState.radiance = bounceRayState.weight * radiance;
    m_pCurrentRenderData->pAccelerationStructure->intersectAny(shadowRay, shadowRayState);
}

bool PathIntegrator::randomBounce(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena)
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

void PathIntegrator::spawnNewPaths(int numPaths)
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

PathIntegrator::HitTaskHandle PathIntegrator::hitTaskHandle() const
{
    return m_hitTask;
}

PathIntegrator::MissTaskHandle PathIntegrator::missTaskHandle() const
{
    return m_missTask;
}

PathIntegrator::AnyHitTaskHandle PathIntegrator::anyHitTaskHandle() const
{
    return m_anyHitTask;
}

PathIntegrator::AnyMissTaskHandle PathIntegrator::anyMissTaskHandle() const
{
    return m_anyMissTask;
}

}
