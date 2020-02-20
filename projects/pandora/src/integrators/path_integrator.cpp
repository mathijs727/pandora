// clang-format off
#include "pandora/graphics_core/sensor.h"
// clang-format on
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
    tasking::TaskGraph* pTaskGraph, tasking::LRUCache* pGeomCache, int maxDepth, int spp, LightStrategy strategy)
    : SamplerIntegrator(pTaskGraph, pGeomCache, maxDepth, spp, strategy)
    , m_hitTask(
          pTaskGraph->addTask<std::tuple<Ray, SurfaceInteraction, RayState>>(
              "PathIntegrator::hit",
              [this](gsl::span<const std::tuple<Ray, SurfaceInteraction, RayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (auto [ray, si, state] : hits) {
                      if (ray.numTopLevelIntersections > 0)
                          m_pCurrentRenderData->pAOVNumTopLevelIntersections->addSplat(
                              state.pixel, ray.numTopLevelIntersections);

                      // TODO: make this use pMemoryResource
                      MemoryArena memoryArena;
                      si.computeScatteringFunctions(ray, memoryArena);
                      this->rayHit(ray, si, state, memoryArena);
                  }
              }))
    , m_missTask(
          pTaskGraph->addTask<std::tuple<Ray, RayState>>(
              "PathIntegrator::miss",
              [this](gsl::span<const std::tuple<Ray, RayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      if (ray.numTopLevelIntersections > 0)
                          m_pCurrentRenderData->pAOVNumTopLevelIntersections->addSplat(
                              state.pixel, ray.numTopLevelIntersections);

                      this->rayMiss(ray, state);
                  }
              }))
    , m_anyHitTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              "PathIntegrator::anyHit",
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : hits) {
                      if (ray.numTopLevelIntersections > 0)
                          m_pCurrentRenderData->pAOVNumTopLevelIntersections->addSplat(
                              state.pixel, ray.numTopLevelIntersections);

                      this->rayAnyHit(ray, state);
                  }
              }))
    , m_anyMissTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              "PathIntegrator::miss",
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      if (ray.numTopLevelIntersections > 0)
                          m_pCurrentRenderData->pAOVNumTopLevelIntersections->addSplat(
                              state.pixel, ray.numTopLevelIntersections);

                      this->rayAnyMiss(ray, state);
                  }
              }))
{
}

void PathIntegrator::rayHit(const Ray& ray, const SurfaceInteraction& si, BounceRayState state, MemoryArena& memoryArena)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    PcgRng rng = state.rng;

    if (state.pathDepth == 0) {
        // Compute emitted light if primary ray hit an area light source
        const Spectrum emitted = si.Le(si.wo);
        if (!isBlack(emitted))
            pSensor->addPixelContribution(state.pixel, state.weight * emitted);
    } else {
        // Next Event Estimation (NEE) samples light sources so random bounce should ignore light source hits (without Multiple Importance Sampling).
        if (si.pSceneObject->pAreaLight || state.pathDepth > m_maxDepth) {
            spawnNewPaths(1);
            return;
        }
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
    glm::vec3 infiniteLightContribution {};
    const auto* pScene = m_pCurrentRenderData->pScene;
    for (const auto* pInfiniteLight : pScene->infiniteLights)
        infiniteLightContribution += pInfiniteLight->Le(ray);

    auto* pSensor = m_pCurrentRenderData->pSensor;
    if (!isBlack(infiniteLightContribution))
        pSensor->addPixelContribution(state.pixel, state.weight * infiniteLightContribution);

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
