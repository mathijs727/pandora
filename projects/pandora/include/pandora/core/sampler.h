#pragma once
#include "glm/glm.hpp"
#include <gsl/gsl>

namespace pandora {

struct CameraSample {
    glm::vec2 pixel;
};

class Sampler {
public:
    Sampler(unsigned samplesPerPixel);

    virtual float get1D() = 0;
    virtual glm::vec2 get2D() = 0;

    virtual bool startNextSample();

    virtual int roundCount(int n) const
    {
        return n;
    }
    void request1DArray(int n);
    void request2DArray(int n);
    void fill1DArray(gsl::span<float> samples);
    void fill2DArray(gsl::span<glm::vec2> samples);

    CameraSample getCameraSample(const glm::ivec2& rasterPixel);

protected:
    const unsigned m_samplesPerPixel;
    unsigned m_currentSampleIndex;
};

}
