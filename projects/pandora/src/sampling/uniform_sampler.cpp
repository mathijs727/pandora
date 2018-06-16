#include "pandora/sampling/uniform_sampler.h"

namespace pandora {
UniformSampler::UniformSampler(unsigned samplesPerPixel)
    : Sampler(samplesPerPixel)
    , m_randomEngine(std::random_device()())
    , m_uniformDistribution(0.0f, 1.0f)
{
}

float UniformSampler::get1D(){
    return m_uniformDistribution(m_randomEngine);
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(m_uniformDistribution(m_randomEngine), m_uniformDistribution(m_randomEngine));
}

}
