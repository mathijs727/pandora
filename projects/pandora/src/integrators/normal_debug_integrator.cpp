#include "pandora/integrators/normal_debug_integrator.h"
#include "pandora/core/perspective_camera.h"
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>

namespace pandora {

NormalDebugIntegrator::NormalDebugIntegrator(const Scene& scene, Sensor& sensor)
    : Integrator(scene, sensor, 1)
    , m_resolution(sensor.getResolution())
{
}

void NormalDebugIntegrator::reset()
{
}

void NormalDebugIntegrator::render(const PerspectiveCamera& camera)
{
    // RAII stopwatch
    auto stopwatch = g_stats.timings.totalRenderTime.getScopedStopwatch();

    // Generate camera rays
    tbb::blocked_range2d<int, int> sensorRange(0, m_resolution.y, 0, m_resolution.x);
    tbb::parallel_for(sensorRange, [&](tbb::blocked_range2d<int, int> localRange) {
        auto rows = localRange.rows();
        auto cols = localRange.cols();
        for (int y = rows.begin(); y < rows.end(); y++) {
            for (int x = cols.begin(); x < cols.end(); x++) {
                glm::ivec2 pixel { x, y };
                NormalDebugIntegratorState rayState { pixel };

                CameraSample cameraSample = { glm::vec2(pixel) + glm::vec2(0.5f) };
                Ray ray = camera.generateRay(cameraSample);
                m_accelerationStructure.placeIntersectRequests(gsl::make_span(&ray, 1), gsl::make_span(&rayState, 1));
            }
        }
    });

    m_accelerationStructure.flush();
}

void NormalDebugIntegrator::rayHit(const Ray& r, SurfaceInteraction si, const NormalDebugIntegratorState& s)
{
    if (s.pixel.x == 780 && s.pixel.y == 155) {
        std::cout << "Plane scene object ID: " << si.sceneObjectMaterial->sceneObjectID << "\n";
    }
    m_sensor.addPixelContribution(s.pixel, glm::abs(glm::normalize(si.normal)));
}

void NormalDebugIntegrator::rayAnyHit(const Ray& r, const NormalDebugIntegratorState& s)
{
}

void NormalDebugIntegrator::rayMiss(const Ray& r, const NormalDebugIntegratorState& s)
{
}

}
