#pragma once
#include "pandora/core/sampler.h"
#include <random>

namespace pandora {
class UniformSampler : public Sampler {
public:
    UniformSampler(unsigned samplesPerPixel);

    float get1D() final;
    glm::vec2 get2D() final;
};
}
