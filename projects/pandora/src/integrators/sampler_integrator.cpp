#include "pandora/integrators/sampler_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include "pandora/utility/memory_arena.h"
#include "utility/fix_visitor.h"
#include <gsl/gsl>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

using namespace pandora::sampler_integrator;

namespace pandora {

SamplerIntegrator::SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp)
    : Integrator(scene, sensor, spp)
    , m_maxDepth(maxDepth)
    , m_cameraThisFrame(nullptr)
{
}

void SamplerIntegrator::render(const PerspectiveCamera& camera)
{
    m_cameraThisFrame = &camera;

    resetSamplers();

    // Generate camera rays
    glm::ivec2 resolution = m_sensor.getResolution();
#ifdef _DEBUG
    for (int y = 0; y < resolution.y; y++) {
#else
    tbb::parallel_for(0, resolution.y, [&](int y) {
#endif
        for (int x = 0; x < resolution.x; x++) {
            // Initialize camera sample for current sample
            auto pixel = glm::ivec2(x, y);
            spawnNextSample(pixel, true);
        }
#ifdef _DEBUG
    }
#else
    });
#endif
    m_sppThisFrame += m_sppPerCall;
}

void SamplerIntegrator::spawnNextSample(const glm::vec2& pixel, bool initialSample)
{
    assert(m_cameraThisFrame != nullptr);

    auto& sampler = getSampler(pixel);
    if (initialSample || sampler.startNextSample()) {
        CameraSample sample = sampler.getCameraSample(pixel);

        ContinuationRayState rayState{ pixel, glm::vec3(1.0f), 0 };
        RayState rayStateVariant = rayState;

        Ray ray = m_cameraThisFrame->generateRay(sample);
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
    }
}

void SamplerIntegrator::specularReflect(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const ContinuationRayState& prevRayState)
{
    // Compute specular reflection wi and BSDF value
    glm::vec3 wo = si.wo;
    BxDFType type = BxDFType(BSDF_REFLECTION | BSDF_SPECULAR);
    auto sample = si.bsdf->sampleF(wo, sampler.get2D(), type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    const glm::vec3& ns = si.shading.normal;
    if (sample->pdf > 0.0f && !isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        // TODO: handle ray differentials
        Ray ray = si.spawnRay(sample->wi);

        ContinuationRayState rayState;
        rayState.bounces = prevRayState.bounces + 1;
        rayState.pixel = prevRayState.pixel;
        rayState.weight = prevRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        RayState rayStateVariant = rayState;
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
    }
}

void SamplerIntegrator::specularTransmit(const SurfaceInteraction& si, Sampler& sampler, MemoryArena& memoryArena, const ContinuationRayState& prevRayState)
{
    // Compute specular reflection wi and BSDF value
    glm::vec3 wo = si.wo;
    BxDFType type = BxDFType(BSDF_TRANSMISSION);
    auto sample = si.bsdf->sampleF(wo, sampler.get2D(), type);
    if (!sample)
        return;

    // Return contribution of specular reflection
    const glm::vec3& ns = si.shading.normal;
    if (sample->pdf > 0.0f && !isBlack(sample->f) && glm::abs(glm::dot(sample->wi, ns)) != 0.0f) {
        // TODO: handle ray differentials
        Ray ray = si.spawnRay(sample->wi);

        ContinuationRayState rayState;
        rayState.bounces = prevRayState.bounces + 1;
        rayState.pixel = prevRayState.pixel;
        rayState.weight = prevRayState.weight * sample->f * glm::abs(glm::dot(sample->wi, ns)) / sample->pdf;

        RayState rayStateVariant = rayState;
        m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
    }
}

void SamplerIntegrator::spawnShadowRay(const Ray& ray, const ContinuationRayState& prevRayState, const Spectrum& radiance)
{
    ShadowRayState rayState;
    rayState.pixel = prevRayState.pixel;
    rayState.radianceOrWeight = prevRayState.weight * radiance;
    rayState.light = nullptr;

    RayState rayStateVariant = rayState;
    m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
}

void SamplerIntegrator::spawnShadowRay(const Ray& ray, const ContinuationRayState& prevRayState, const Spectrum& weight, const Light& light)
{
    ShadowRayState rayState;
    rayState.pixel = prevRayState.pixel;
    rayState.radianceOrWeight = prevRayState.weight * weight;
    rayState.light = &light;

    RayState rayStateVariant = rayState;
    m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
}

}
