#include "pandora/core/sampler_integrator.h"
#include "pandora/core/scene.h"
#include "pandora/core/sensor.h"

namespace pandora {

SamplerIntegrator::SamplerIntegrator(int maxDepth, const Scene& scene, Sensor& sensor)
    : m_maxDepth(maxDepth)
    , m_scene(scene)
    , m_samplers(sensor.getResolution().x * sensor.getResolution().y, 4)
    , m_sensor(sensor)
    , m_accelerationStructure(scene.getSceneObjects(), [this](const Ray& r, const SurfaceInteraction& i, const PathState& s, const auto& h) {
        
    })
{
}

void SamplerIntegrator::render(const PerspectiveCamera& camera)
{
}

Sampler& SamplerIntegrator::getSampler(const glm::ivec2& pixel)
{
    // TODO: insert return statement here
}

}
