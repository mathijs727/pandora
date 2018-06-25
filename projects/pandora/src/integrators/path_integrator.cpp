#include "pandora/integrators/path_integrator.h"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/sampler.h"
#include "pandora/core/sensor.h"
#include <gsl/gsl>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

PathIntegrator::PathIntegrator(int maxDepth, const Scene& scene, Sensor& sensor, int spp)
    : SamplerIntegrator(maxDepth, scene, sensor, spp)
{
}

void PathIntegrator::rayHit(const Ray& r, const SurfaceInteraction& si, const RayState& s, const EmbreeInsertHandle& h)
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

void PathIntegrator::rayMiss(const Ray& r, const RayState& s)
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
