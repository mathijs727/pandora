#include "pandora/sampling/sampler.h"

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

}
