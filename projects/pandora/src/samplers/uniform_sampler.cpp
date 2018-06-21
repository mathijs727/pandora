#include "pandora/samplers/uniform_sampler.h"
#include <atomic>
#include <mutex>

// std::default_random_engine is very large (5KB). We can save a lot of memory by only storing one instance per thread.
static thread_local std::uniform_real_distribution<float> uniformDistribution = std::uniform_real_distribution<float>(0.0f, 1.0f);
static thread_local std::default_random_engine randomEngine = std::default_random_engine(std::random_device()());

namespace pandora {
UniformSampler::UniformSampler(unsigned samplesPerPixel)
    : Sampler(samplesPerPixel)
{
}

float UniformSampler::get1D()
{
    return uniformDistribution(randomEngine);
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(uniformDistribution(randomEngine), uniformDistribution(randomEngine));
}

}
