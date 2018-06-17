#include "pandora/core/path_integrator.h"
#include "pandora/materials/lambert_material.h"
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor)
    : m_maxDepth(maxDepth)
    , m_scene(scene)
    , m_samplers(sensor.getResolution().x * sensor.getResolution().y, 4)
    , m_sensor(sensor)
    , m_accelerationStructure(scene.getSceneObjects(), [this](const Ray& r, const SurfaceInteraction& i, const PathState& s, const auto& h) {
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
    })
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

            PathState pathState{ pixel, glm::vec3(1.0f), 0 };
            Ray ray = camera.generateRay(sample);
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&pathState, 1), gsl::make_span(&ray, 1));
        }
#ifdef _DEBUG
    }
#else
    });
#endif
}

std::variant<PathIntegrator::NewRays, glm::vec3> PathIntegrator::performShading(const SurfaceInteraction& intersection, gsl::span<glm::vec2> samples) const
{
    if (intersection.sceneObject != nullptr) {
        const Material& material = *intersection.sceneObject->material.get();
        auto sampleResult = material.sampleBSDF(intersection, samples);

        glm::vec3 continuationWeight = sampleResult.multiplier / sampleResult.pdf;
        assert(!glm::isnan(continuationWeight.x) && !glm::isnan(continuationWeight.y) && !glm::isnan(continuationWeight.z));
        Ray continuationRay = Ray(intersection.position + RAY_EPSILON * intersection.normal, sampleResult.out);
        return NewRays{ continuationWeight, continuationRay };
    } else {
        glm::vec3 radiance = glm::vec3(0.0f);

        auto lights = m_scene.getInfiniteLIghts();
        for (const auto& light : lights) {
            radiance += light->Le(-intersection.wo);
        }

        return radiance;
    }
}

Sampler& PathIntegrator::getSampler(const glm::ivec2& pixel)
{
    return m_samplers[pixel.y * m_sensor.getResolution().x + pixel.x];
}
}
