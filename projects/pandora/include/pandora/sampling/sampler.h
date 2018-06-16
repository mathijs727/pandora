#pragma once
#include "glm/glm.hpp"

namespace pandora
{

struct CameraSample {
    glm::vec2 pixel;
};

class Sampler
{
public:
    Sampler(unsigned samplesPerPixel);

    virtual float get1D() = 0;
    virtual glm::vec2 get2D() = 0;

    CameraSample getCameraSample(const glm::ivec2& rasterPixel);
protected:
    const unsigned m_samplesPerPixel;
    unsigned m_currentSampleIndex;
};

}
