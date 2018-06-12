#include "pandora/core/path_integrator.h"
#include "pandora/shading/lambert_material.h"
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

PathIntegrator::PathIntegrator(int maxDepth, PerspectiveCamera& camera)
    : m_maxDepth(maxDepth)
    , m_camera(camera)
{
}

void PathIntegrator::render(const Scene& scene)
{
    // Optional: preprocess

    // Allocate memory for path states
    glm::ivec2 resolution = m_camera.getSensor().getResolution();
    int pixelCount = resolution.x * resolution.y;
    m_rayQueue1.resize(pixelCount);
    m_rayQueue2.reserve(pixelCount);
    auto& activeRayQueue = m_rayQueue1;
    auto& resultRayQueue = m_rayQueue2;

    // Generate camera rays
    float widthF = static_cast<float>(resolution.x);
    float heightF = static_cast<float>(resolution.y);
    tbb::parallel_for(0, resolution.y, [&](int y) {
        //for (int y = 0; y < resolution.y; y++) { // Not parallel because rayQueue.push_back is not thread safe
        for (int x = 0; x < resolution.x; x++) {
            // TODO: abstract in camera sampler and make this a while loop (multiple samples per pixel)

            // Initialize camera sample for current sample
            auto pixel = glm::ivec2(x, y);
            auto pixelScreenCoords = glm::vec2(x / widthF, y / heightF);
            CameraSample sample = CameraSample(pixelScreenCoords);

            PathState pathState(pixel);
            Ray ray = m_camera.generateRay(CameraSample(pixelScreenCoords), pathState);

            // TODO:
            // rayQueue.push(ray)
            int idx = y * resolution.x + x;
            activeRayQueue[idx] = ray;
        }
        //}
    });

    auto& sensor = m_camera.getSensor();
    while (activeRayQueue.size() > 0) {
        resultRayQueue.clear();

        // TODO: make this run on a secondary thread, communicating through channels
        const auto& accelerator = scene.getAccelerator();
        //for (auto& ray : activeRayQueue) {
        tbb::parallel_for(0, (int)activeRayQueue.size(), [&](int i) {
            auto ray = activeRayQueue[i];

            IntersectionData intersectionData;
            accelerator.intersect(ray, intersectionData);

            auto shadingResult = performShading(scene, ray, intersectionData);
            if (std::holds_alternative<Continuation>(shadingResult)) {
                // Continue path
                auto [continuationRay] = std::get<Continuation>(shadingResult);
                resultRayQueue.push_back(continuationRay);
                //m_rayQueue2.push_back(shadowRay);
            } else {
                // End of path: received radiance
                auto radiance = std::get<glm::vec3>(shadingResult);
                sensor.addPixelContribution(ray.pathState.pixel, radiance);
            }
        });
        //}

        std::swap(activeRayQueue, resultRayQueue);
    }
}

std::variant<PathIntegrator::Continuation, glm::vec3> PathIntegrator::performShading(const Scene& scene, const Ray& ray, const IntersectionData& intersection)
{
    const float epsilon = 0.00001f;
    const LambertMaterial material(glm::vec3(0.6f, 0.4f, 0.5f));
    const glm::vec3 backgroundColor = glm::vec3(1.0);

    auto pathState = ray.pathState;
    if (pathState.depth < 8 && intersection.objectHit != nullptr) {
        auto shadingResult = material.sampleBSDF(intersection);

        pathState.weight = pathState.weight * shadingResult.weight / shadingResult.pdf;
        pathState.depth++;
        Ray continuationRay = Ray(intersection.position - epsilon * intersection.incident, shadingResult.out, pathState);
        return Continuation{ continuationRay };
    } else {
        if (pathState.depth == 0)
            return glm::vec3(0);
        else
            return pathState.weight * backgroundColor;
    }
}

}
