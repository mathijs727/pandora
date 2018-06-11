#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_frameBuffer(std::make_unique<glm::vec3[]>(width * height))
{
    clear(glm::vec3(0.0f));
}

void Sensor::clear(glm::vec3 color)
{
    std::fill(m_frameBuffer.get(), m_frameBuffer.get() + m_width * m_height, color);
}

void Sensor::addPixelContribution(glm::ivec2 pixel, glm::vec3 value)
{
    m_frameBuffer[getIndex(pixel.x, pixel.y)] += value;
}

gsl::not_null<const glm::vec3*> Sensor::getFramebufferRaw() const
{
    return gsl::not_null<const glm::vec3*>(m_frameBuffer.get());
}

int Sensor::getIndex(int x, int y) const
{
    return x * m_height + y;
}

}
