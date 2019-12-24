// clang-format off
#include "pandora/graphics_core/sensor.h"
// clang-format on
#include "pandora/integrators/naive_direct_lighting_integrator.h"
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
    tasking::TaskGraph* pTaskGraph, tasking::LRUCache* pGeomCache, int maxDepth, int spp, LightStrategy strategy)
    : SamplerIntegrator(pTaskGraph, pGeomCache, maxDepth, spp, strategy)
    , m_hitTask(
          pTaskGraph->addTask<std::tuple<Ray, SurfaceInteraction, RayState>>(
              "DirectLightingIntegrator::hit",
              [this](gsl::span<const std::tuple<Ray, SurfaceInteraction, RayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (auto [ray, si, state] : hits) {
                      // TODO: make this use pMemoryResource
                      MemoryArena memoryArena;
                      si.computeScatteringFunctions(ray, memoryArena);
                      this->rayHit(ray, si, state, memoryArena);
                  }
              }))
    , m_missTask(
          pTaskGraph->addTask<std::tuple<Ray, RayState>>(
              "DirectLightingIntegrator::miss",
              [this](gsl::span<const std::tuple<Ray, RayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      this->rayMiss(ray, state);
                  }
              }))
    , m_anyHitTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              "DirectLightingIntegrator::anyHit",
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> hits, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : hits) {
                      this->rayAnyHit(ray, state);
                  }
              }))
    , m_anyMissTask(
          pTaskGraph->addTask<std::tuple<Ray, AnyRayState>>(
              "DirectLightingIntegrator::anyMiss",
              [this](gsl::span<const std::tuple<Ray, AnyRayState>> misses, std::pmr::memory_resource* pMemoryResource) {
                  for (const auto& [ray, state] : misses) {
                      this->rayAnyMiss(ray, state);
                  }
              }))
{
}

void DirectLightingIntegrator::rayHit(const Ray& ray, const SurfaceInteraction& si, BounceRayState state, MemoryArena& memoryArena)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    PcgRng rng = state.rng;

    // Compute emitted light if ray hit an area light source
    const Spectrum emitted = si.Le(si.wo);
    if (!isBlack(emitted))
        pSensor->addPixelContribution(state.pixel, state.weight * emitted);

    // Sample direct light using Next Event Estimation (NEE)
    if (m_strategy == LightStrategy::UniformSampleAll)
        uniformSampleAllLights(si, state, rng);
    else
        uniformSampleOneLight(si, state, rng);

    // TODO: specular bounce rays will also spawn new paths which might overload the system
    // Next Event Estimation (NEE) samples light sources so random bounce should ignore it.
    if (state.pathDepth + 1 < m_maxDepth) {
        specularReflect(si, state, rng, memoryArena);
        specularTransmit(si, state, rng, memoryArena);
    }

    spawnNewPaths(1);
}

void DirectLightingIntegrator::rayMiss(const Ray& ray, const BounceRayState& state)
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

void DirectLightingIntegrator::rayAnyHit(const Ray& ray, const ShadowRayState& state)
{
}

void DirectLightingIntegrator::rayAnyMiss(const Ray& ray, const ShadowRayState& state)
{
    auto* pSensor = m_pCurrentRenderData->pSensor;
    pSensor->addPixelContribution(state.pixel, state.radiance);
}

void DirectLightingIntegrator::specularReflect(const SurfaceInteraction& si, const RayState& prevRayState, PcgRng& rng, MemoryArena& memoryArena)
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

}
