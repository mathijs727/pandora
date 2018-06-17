#include "pandora/samplers/uniform_sampler.h"
#include <mutex>

static std::random_device s_randomDevice = std::random_device();
static std::mutex s_randomDeviceMutex;

namespace pandora {
UniformSampler::UniformSampler(unsigned samplesPerPixel)
    : Sampler(samplesPerPixel)
    , m_uniformDistribution(0.0f, 1.0f)
{
    std::lock_guard<std::mutex> l(s_randomDeviceMutex);
    m_randomEngine = std::default_random_engine(s_randomDevice());
}

float UniformSampler::get1D()
{
    return m_uniformDistribution(m_randomEngine);
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(m_uniformDistribution(m_randomEngine), m_uniformDistribution(m_randomEngine));
}

}
