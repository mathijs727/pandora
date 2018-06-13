#include "pandora/core/path_integrator.h"
#include "pandora/shading/lambert_material.h"
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor)
    : m_maxDepth(maxDepth)
    , m_scene(scene)
    , m_sensor(sensor)
    , m_accelerationStructure(scene.getMeshes(), [this](const Ray& r, const IntersectionData& i, const PathState& s, const auto& h) {
        /*if (i.objectHit != nullptr) {
            if (s.depth < 2) {
                PathState pathState = s;
                pathState.depth++;
                Ray ray = r;
                m_accelerationStructure.placeIntersectRequests(gsl::make_span(&pathState, 1), gsl::make_span(&ray, 1));
            } else {
                m_sensor.addPixelContribution(s.pixel, glm::vec3(1));
            }
        }*/
        if (s.depth > m_maxDepth)
            return;

        auto shadingResult = performShading(s.weight, r, i);

        if (std::holds_alternative<NewRays>(shadingResult)) {
            // Continue path
            auto [continuationWeight, continuationRay] = std::get<NewRays>(shadingResult);
            PathState newState = s;
            newState.depth++;
            newState.weight *= continuationWeight;
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&newState, 1), gsl::make_span(&continuationRay, 1));
        } else {
            if (s.depth > 0) {
                // End of path: received radiance
                auto radiance = std::get<glm::vec3>(shadingResult);
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
    tbb::parallel_for(0, resolution.y, [&](int y) {
        //for (int y = 0; y < resolution.y; y++) { // Not parallel because rayQueue.push_back is not thread safe
        for (int x = 0; x < resolution.x; x++) {
            // TODO: abstract in camera sampler and make this a while loop (multiple samples per pixel)

            // Initialize camera sample for current sample
            auto pixel = glm::ivec2(x, y);
            auto pixelScreenCoords = glm::vec2(x / resolutionF.x, y / resolutionF.y);
            CameraSample sample = CameraSample(pixelScreenCoords);

            PathState pathState{ pixel, glm::vec3(1.0f), 0 };
            Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));
            //Ray ray(glm::vec3(0, 0, -5), glm::vec3(0, 0, 1));
            //m_sensor.addPixelContribution(pixel, glm::vec3(1, 0, 0));
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&pathState, 1), gsl::make_span(&ray, 1));
        }
        //}
    });
}

std::variant<PathIntegrator::NewRays, glm::vec3> PathIntegrator::performShading(glm::vec3 weight, const Ray& ray, const IntersectionData& intersection) const
{
    const float epsilon = 0.00001f;
    const LambertMaterial material(glm::vec3(0.6f, 0.4f, 0.5f));
    const glm::vec3 backgroundColor = glm::vec3(1.0);

    if (intersection.objectHit != nullptr) {
        auto shadingResult = material.sampleBSDF(intersection);

        glm::vec3 continuationWeight = weight * shadingResult.weight / shadingResult.pdf;
        Ray continuationRay = Ray(intersection.position - epsilon * intersection.incident, shadingResult.out);
        return NewRays{ continuationWeight, continuationRay };
    } else {
        return weight * backgroundColor;
    }
}
}
