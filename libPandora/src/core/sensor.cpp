#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_frameBuffer = std::make_unique<Vec3f[]>(width * height);
}

void Sensor::clear(Vec3f color)
{
    std::fill(std::begin(m_frameBuffer.get()), std::end(m_frameBuffer.get()), color);
}

void Sensor::addPixelContribution(int x, int y, Vec3f value)
{
}

void Sensor::drawOpenGL()
{
}
}