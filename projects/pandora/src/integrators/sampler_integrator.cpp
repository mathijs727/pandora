#include "pandora/integrators/sampler_integrator.h"
#include "pandora/graphics_core/bxdf.h"
#include "pandora/graphics_core/material.h"
#include "pandora/graphics_core/perspective_camera.h"
#include "pandora/graphics_core/scene.h"
#include "pandora/graphics_core/sensor.h"
#include "pandora/samplers/rng/pcg.h"
#include "pandora/utility/math.h"

namespace pandora {

SamplerIntegrator::SamplerIntegrator(tasking::TaskGraph* pTaskGraph, int maxDepth, int spp, LightStrategy strategy)
    : m_pTaskGraph(pTaskGraph)
    , m_maxDepth(maxDepth)
    , m_maxSpp(spp)
    , m_strategy(strategy)
{
}

void SamplerIntegrator::render(const PerspectiveCamera& camera, Sensor& sensor, const Scene& scene, const Accel& accel)
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

void SamplerIntegrator::uniformSampleAllLights(
    const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng)
{
    const auto* pScene = m_pCurrentRenderData->pScene;
    for (const auto& pLight : pScene->lights) {
        estimateDirect(si, *pLight, 1.0f, bounceRayState, rng);
    }
}

void SamplerIntegrator::uniformSampleOneLight(
    const SurfaceInteraction& si, const BounceRayState& bounceRayState, PcgRng& rng)
{
    const auto* pScene = m_pCurrentRenderData->pScene;

    // Randomly choose a single light to sample
    uint32_t numLights = static_cast<uint32_t>(pScene->lights.size());
    if (numLights == 0)
        return;

    uint32_t lightNum = std::min(rng.uniformU32(), numLights - 1);
    const auto& pLight = pScene->lights[lightNum];

    estimateDirect(si, *pLight, static_cast<float>(numLights), bounceRayState, rng);
}

void SamplerIntegrator::estimateDirect(
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

void SamplerIntegrator::spawnShadowRay(const Ray& shadowRay, PcgRng& rng, const BounceRayState& bounceRayState, const Spectrum& radiance)
{
    ShadowRayState shadowRayState;
    shadowRayState.pixel = bounceRayState.pixel;
    shadowRayState.radiance = bounceRayState.weight * radiance;
    m_pCurrentRenderData->pAccelerationStructure->intersectAny(shadowRay, shadowRayState);
}

void SamplerIntegrator::spawnNewPaths(int numPaths)
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

        const glm::vec2 resolution = m_pCurrentRenderData->fResolution;
        const glm::vec2 cameraSample = glm::vec2(x, y) / resolution + rayState.rng.uniformFloat2();

        const Ray cameraRay = pRenderData->pCamera->generateRay(cameraSample);
        pRenderData->pAccelerationStructure->intersect(cameraRay, rayState);
    }
}

}
