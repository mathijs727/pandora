#pragma once
#include "pandora/graphics_core/sampler.h"
#include "pandora/samplers/rng/pcg.h"
#include <random>

namespace pandora {
class UniformSampler : public Sampler {
public:
    UniformSampler(unsigned rngSeed);

    float get1D() final;
    glm::vec2 get2D() final;

private:
    PcgRng m_rng;
};
}
