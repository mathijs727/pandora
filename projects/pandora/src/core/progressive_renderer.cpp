#include "pandora/core/progressive_renderer.h"
#include "pandora/shading/lambert_material.h"
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>

namespace pandora {

ProgressiveRenderer::ProgressiveRenderer(const Scene& scene, Sensor& sensor)
    : m_spp(1)
    , m_sensor(sensor)
    , m_integrator(8, scene, sensor)
{
}

void ProgressiveRenderer::clear()
{
    m_spp = 1;
    m_sensor.clear(glm::vec3(0));
}

void ProgressiveRenderer::incrementalRender(const PerspectiveCamera& camera)
{
    m_integrator.render(camera);
    m_spp++;
}

int ProgressiveRenderer::getSampleCount() const
{
    return m_spp;
}

}
