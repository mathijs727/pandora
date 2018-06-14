#include "pandora/core/sensor.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(glm::ivec2 resolution)
    : m_resolution(resolution)
    , m_frameBuffer(std::make_unique<glm::vec3[]>(resolution.x * resolution.y))
{
    clear(glm::vec3(0.0f));
}

void Sensor::clear(glm::vec3 color)
{
    std::fill(m_frameBuffer.get(), m_frameBuffer.get() + m_resolution.x * m_resolution.y, color);
}

void Sensor::addPixelContribution(glm::ivec2 pixel, glm::vec3 value)
{
    m_frameBuffer[getIndex(pixel.x, pixel.y)] += value;
}

glm::ivec2 Sensor::getResolution() const
{
    return m_resolution;
}

gsl::not_null<const glm::vec3*> Sensor::getFramebufferRaw() const
{
    return gsl::not_null<const glm::vec3*>(m_frameBuffer.get());
}

int Sensor::getIndex(int x, int y) const
{
    return y * m_resolution.x + x;
}

}
