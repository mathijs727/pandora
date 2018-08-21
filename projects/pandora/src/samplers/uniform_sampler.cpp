#include "pandora/samplers/uniform_sampler.h"
//#include "rng/pcg.h"

// std::default_random_engine is very large (5KB). We can save a lot of memory by only storing one instance per thread.
//static thread_local std::uniform_real_distribution<float> uniformDistribution = std::uniform_real_distribution<float>(0.0f, 1.0f);
//static thread_local std::default_random_engine randomEngine = std::default_random_engine(std::random_device()());

namespace pandora {

//static thread_local PcgRng randomNumberGenerator;

UniformSampler::UniformSampler(unsigned samplesPerPixel, unsigned rngSeed)
    : Sampler(samplesPerPixel)
    , m_rng(rngSeed)
{
}

float UniformSampler::get1D()
{
    return m_rng.uniformFloat();
    //return uniformDistribution(randomEngine);
    //return 0.5f;
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(get1D(), get1D());
}

}
