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

void SamplerIntegrator::rayHit(const Ray& r, const SurfaceInteraction& siRef, const RayState& s, const EmbreeInsertHandle& h)
{
    SurfaceInteraction si = siRef;

    if (std::holds_alternative<ContinuationRayState>(s)) {
        const auto& rayState = std::get<ContinuationRayState>(s);

        // TODO
        auto& sampler = getSampler(rayState.pixel);
        std::array samples = { sampler.get2D() };

        // Initialize common variables for Whitted integrator
        glm::vec3 n = si.shading.normal;
        glm::vec3 wo = si.wo;

        // Compute scattering functions for surface interaction
        MemoryArena arena;
        si.computeScatteringFunctions(r, arena);

        // Compute emitted light if ray hit an area light source
        glm::vec3 radiance(0.0f);
        radiance += si.lightEmitted(wo);

        // Add contribution of each light source
        for (const auto& light : m_scene.getLights()) {
            auto bsdfSample = si.bsdf->sampleF(wo, sampler.get2D());
            if (bsdfSample)
            {
                Spectrum f = si.bsdf->f(wo, bsdfSample->wi);
                if (!isBlack(f)) {
                    Spectrum radiance = f * light->Le(bsdfSample->wi) * glm::abs(glm::dot(bsdfSample->wi, n)) / bsdfSample->pdf;
                    Ray visibilityRay = si.spawnRay(bsdfSample->wi);
                    spawnShadowRay(visibilityRay, rayState, radiance);
                }
            }

            /*auto lightSample = light->sampleLi(si, sampler.get2D());
            if (lightSample.isBlack() || lightSample.pdf == 0.0f)
                continue;

            Spectrum f = si.bsdf->f(wo, lightSample.wi);
            if (!isBlack(f)) {
                Spectrum radiance = f * lightSample.radiance * glm::abs(glm::dot(lightSample.wi, n)) / lightSample.pdf;
                spawnShadowRay(lightSample.visibilityRay, rayState, radiance);
            }*/
        }

        if (rayState.depth + 1 < m_maxDepth) {
            // Trace rays for specular reflection and refraction
            //specularReflect(si, sampler, arena, rayState);
            //specularTransmit(si, sampler, arena, rayState);
        }
    } else if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);
        // Do nothing, in shadow
        assert(false); // TEMPORARY: no self shadowing on a sphere (or other convex shape)
    }
}

void SamplerIntegrator::rayMiss(const Ray& r, const RayState& s)
{
    if (std::holds_alternative<ShadowRayState>(s)) {
        const auto& rayState = std::get<ShadowRayState>(s);
        assert(!std::isnan(glm::dot(rayState.contribution, rayState.contribution)) && glm::dot(rayState.contribution, rayState.contribution) > 0.0f);
        m_sensor.addPixelContribution(rayState.pixel, rayState.contribution);

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
        rayState.depth = prevRayState.depth + 1;
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
        rayState.depth = prevRayState.depth + 1;
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
    rayState.contribution = prevRayState.weight * radiance;

    RayState rayStateVariant = rayState;
    m_accelerationStructure.placeIntersectRequests(gsl::make_span(&rayStateVariant, 1), gsl::make_span(&ray, 1));
}

}
