#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_frameBuffer(std::make_unique<Vec3f[]>(width * height))
{
    clear(Vec3f(0.0f));
}

void Sensor::clear(Vec3f color)
{
    std::fill(m_frameBuffer.get(), m_frameBuffer.get() + m_width * m_height, color);
}

void Sensor::addPixelContribution(Vec2i pixel, Vec3f value)
{
    m_frameBuffer[getIndex(pixel.x, pixel.y)] += value;
}

gsl::not_null<const Vec3f*> Sensor::getFramebufferRaw() const
{
    return gsl::not_null<const Vec3f*>(m_frameBuffer.get());
}

int Sensor::getIndex(int x, int y) const
{
    return x * m_height + y;
}

}
