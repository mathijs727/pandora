#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_frameBuffer(width * height)
{
}

void Sensor::clear(Vec3f color)
{
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), color);
}

void Sensor::addPixelContribution(Vec2i pixel, Vec3f value)
{
    int index = pixel.y * m_width + pixel.x;
    m_frameBuffer[index] += value;
}

gsl::multi_span<const Vec3f, gsl::dynamic_range, gsl::dynamic_range> Sensor::getFramebuffer() const
{
    return gsl::as_multi_span(gsl::as_multi_span(m_frameBuffer), gsl::dim(m_width), gsl::dim(m_height));
}
}