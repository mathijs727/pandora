#include "pandora/samplers/uniform_sampler.h"
#include "rng/pcg.h"
#include <atomic>
#include <mutex>

// std::default_random_engine is very large (5KB). We can save a lot of memory by only storing one instance per thread.
//static thread_local std::uniform_real_distribution<float> uniformDistribution = std::uniform_real_distribution<float>(0.0f, 1.0f);
//static thread_local std::default_random_engine randomEngine = std::default_random_engine(std::random_device()());

namespace pandora {

static thread_local PcgRng m_randomNumberGenerator;

UniformSampler::UniformSampler(unsigned samplesPerPixel)
    : Sampler(samplesPerPixel)
{
}

float UniformSampler::get1D()
{
	return m_randomNumberGenerator.uniformFloat();
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(get1D(), get1D());
}

}
