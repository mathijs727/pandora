#include "pandora/core/sampler.h"

namespace pandora {

Sampler::Sampler(unsigned samplesPerPixel)
    : m_samplesPerPixel(samplesPerPixel)
    , m_currentSampleIndex(0)
{
}

CameraSample Sampler::getCameraSample(const glm::ivec2& rasterPixel)
{
    CameraSample cameraSample;
    cameraSample.pixel = glm::vec2(rasterPixel) + get2D();
    return cameraSample;
}

void Sampler::reset()
{
    m_currentSampleIndex = 0;
}

bool Sampler::startNextSample()
{
    // Rest array offsets for next pixel sample
    return ++m_currentSampleIndex < m_samplesPerPixel;
}
}
