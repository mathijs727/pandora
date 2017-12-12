#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

typedef boost::multi_array_types::index_range range;

namespace pandora {

Sensor::Sensor(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_frameBuffer(boost::extents[width][height])
{
    clear(Vec3f(0.0f));
}

void Sensor::clear(Vec3f color)
{
    std::fill(m_frameBuffer.origin(), m_frameBuffer.origin() + m_frameBuffer.num_elements(), color);
}

void Sensor::addPixelContribution(Vec2i pixel, Vec3f value)
{
    m_frameBuffer[pixel.x][pixel.y] += value;
}

const Sensor::FrameBufferConstArrayView Sensor::getFramebufferView() const
{
    return m_frameBuffer[boost::indices[range{ 0, m_width }][range{ 0, m_height }]];
}

gsl::not_null<const Vec3f*> Sensor::getFramebufferRaw() const
{
    return m_frameBuffer.data();
}
}