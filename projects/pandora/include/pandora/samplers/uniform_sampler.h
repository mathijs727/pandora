#pragma once
#include "pandora/core/sampler.h"
#include <random>

namespace pandora {
class UniformSampler : public Sampler {
public:
    UniformSampler(unsigned samplesPerPixel);

    float get1D() final;
    glm::vec2 get2D() final;

private:
    std::default_random_engine m_randomEngine;
    std::uniform_real_distribution<float> m_uniformDistribution;
};
}
