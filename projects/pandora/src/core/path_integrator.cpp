#include "pandora/core/path_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include "pandora/materials/lambert_material.h"
#include <gsl/gsl>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

/*
auto& sampler = getSampler(s.pixel);
        std::array samples = { sampler.get2D() };
        auto shadingResult = performShading(i, samples);
        if (std::holds_alternative<NewRays>(shadingResult)) {
            // Continue path
            auto [multiplier, continuationRay] = std::get<NewRays>(shadingResult);
            PathState newState = s;
            if (newState.depth++ > m_maxDepth)
                return;
            newState.weight *= multiplier;
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&newState, 1), gsl::make_span(&continuationRay, 1));
        } else {
            if (s.depth > 0) {
                // End of path: received radiance
                auto radiance = s.weight * std::get<glm::vec3>(shadingResult);
                m_sensor.addPixelContribution(s.pixel, radiance);
            }
        }

*/

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor)
    : Integrator(scene, sensor)
    , m_maxDepth(maxDepth)
{
}

void PathIntegrator::render(const PerspectiveCamera& camera)
{
    // Generate camera rays
    glm::ivec2 resolution = m_sensor.getResolution();
    glm::vec2 resolutionF = resolution;
#ifdef _DEBUG
    for (int y = 0; y < resolution.y; y++) {
#else
    tbb::parallel_for(0, resolution.y, [&](int y) {
#endif
        for (int x = 0; x < resolution.x; x++) {
            // Initialize camera sample for current sample
            auto pixel = glm::ivec2(x, y);
            auto& sampler = getSampler(pixel);
            CameraSample sample = sampler.getCameraSample(pixel);

            PathIntegratorState pathState{ pixel, glm::vec3(1.0f), 0 };
            Ray ray = camera.generateRay(sample);
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&pathState, 1), gsl::make_span(&ray, 1));
        }
#ifdef _DEBUG
    }
#else
    });
#endif
}

void PathIntegrator::rayHit(const Ray& r, const SurfaceInteraction& si, const PathIntegratorState& s, const EmbreeInsertHandle& h)
{
    // TODO
    auto& sampler = getSampler(s.pixel);
    std::array samples = { sampler.get2D() };

    const Material& material = *si.sceneObject->material.get();
    auto sampleResult = material.sampleBSDF(si, samples);

    glm::vec3 continuationWeight = s.weight * sampleResult.multiplier / sampleResult.pdf;
    assert(!glm::isnan(continuationWeight.x) && !glm::isnan(continuationWeight.y) && !glm::isnan(continuationWeight.z));

    PathIntegratorState state;
    state.depth = s.depth + 1;
    state.pixel = s.pixel;
    state.weight = continuationWeight;

    Ray ray = Ray(si.position + RAY_EPSILON * si.normal, sampleResult.out);
    m_accelerationStructure.placeIntersectRequests(gsl::make_span(&state, 1), gsl::make_span(&ray, 1));
}

void PathIntegrator::rayMiss(const Ray& r, const PathIntegratorState& s)
{
    // End of path: received radiance
    glm::vec3 radiance = glm::vec3(0.0f);

    auto lights = m_scene.getInfiniteLIghts();
    for (const auto& light : lights) {
        radiance += light->Le(r.direction);
    }

    m_sensor.addPixelContribution(s.pixel, s.weight * radiance);
}

}
