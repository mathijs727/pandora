#pragma once
#include "glm/glm.hpp"
#include <gsl/gsl>

namespace pandora {

struct CameraSample {
    glm::vec2 pixel;
};

class Sampler {
public:
    virtual ~Sampler() {}

    virtual float get1D() = 0;
    virtual glm::vec2 get2D() = 0;

    CameraSample getCameraSample(const glm::ivec2& rasterPixel);
};

}
