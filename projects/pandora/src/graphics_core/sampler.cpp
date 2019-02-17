#include "pandora/core/sampler.h"

namespace pandora {

CameraSample Sampler::getCameraSample(const glm::ivec2& rasterPixel)
{
    CameraSample cameraSample;
    cameraSample.pixel = glm::vec2(rasterPixel) + get2D();
    return cameraSample;
}

}
