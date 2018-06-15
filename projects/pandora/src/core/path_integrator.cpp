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
    , m_accelerationStructure(scene.getSceneObjects(), [this](const Ray& r, const SurfaceInteraction& i, const PathState& s, const auto& h) {
        //if (i.sceneObject != nullptr)
        //    m_sensor.addPixelContribution(s.pixel, glm::abs(i.geometricNormal));

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
#ifdef _DEBUG
    for (int y = 0; y < resolution.y; y++) {
#else
    tbb::parallel_for(0, resolution.y, [&](int y) {
#endif
        for (int x = 0; x < resolution.x; x++) {
            // TODO: abstract in camera sampler and make this a while loop (multiple samples per pixel)

            // Initialize camera sample for current sample
            auto pixel = glm::ivec2(x, y);
            auto pixelScreenCoords = glm::vec2(x / resolutionF.x, y / resolutionF.y);
            CameraSample sample = CameraSample(pixelScreenCoords);

            PathState pathState{ pixel, glm::vec3(1.0f), 0 };
            Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));
            m_accelerationStructure.placeIntersectRequests(gsl::make_span(&pathState, 1), gsl::make_span(&ray, 1));
        }
#ifdef _DEBUG
    }
#else
    });
#endif
}

std::variant<PathIntegrator::NewRays, glm::vec3> PathIntegrator::performShading(glm::vec3 weight, const Ray& ray, const SurfaceInteraction& intersection) const
{
    const glm::vec3 backgroundColor = glm::vec3(1.0);

    if (intersection.sceneObject != nullptr) {
        const Material& material = *intersection.sceneObject->material.get();
        auto shadingResult = material.sampleBSDF(intersection);

        glm::vec3 continuationWeight = weight * shadingResult.weight / shadingResult.pdf;
        assert(!glm::isnan(continuationWeight.x) && !glm::isnan(continuationWeight.y) && !glm::isnan(continuationWeight.z));
        Ray continuationRay = Ray(intersection.position - RAY_EPSILON * intersection.wo, shadingResult.out);
        return NewRays{ continuationWeight, continuationRay };
    } else {
        return weight * backgroundColor;
    }
}
}
