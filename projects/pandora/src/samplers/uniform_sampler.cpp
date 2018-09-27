#include "pandora/samplers/uniform_sampler.h"

namespace pandora {

UniformSampler::UniformSampler(unsigned rngSeed)
    : m_rng(rngSeed)
{
}

float UniformSampler::get1D()
{
    return m_rng.uniformFloat();
}

glm::vec2 UniformSampler::get2D()
{
    return glm::vec2(get1D(), get1D());
}

}
