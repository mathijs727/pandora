#include "pandora/graphics_core/sensor.h"
#include "pandora/utility/error_handling.h"
#include <iterator>
#include <memory>

namespace pandora {

Sensor::Sensor(glm::ivec2 resolution)
    : m_resolution(resolution)
    , m_frameBuffer(resolution.x * resolution.y)
    , m_frameBufferCopy(resolution.x * resolution.y)
{
    clear(glm::vec3(0.0f));
}

void Sensor::clear(glm::vec3 color)
{
    std::for_each(std::begin(m_frameBuffer), std::end(m_frameBuffer), [&](auto& atomicPixelColor) { atomicPixelColor.store(color); });
}

void Sensor::addPixelContribution(glm::ivec2 pixel, glm::vec3 color)
{
    auto& pixelVar = m_frameBuffer[getIndex(pixel.x, pixel.y)];

    //ALWAYS_ASSERT(!glm::any(glm::isnan(color) || glm::isinf(color)));
    if (glm::any(glm::isnan(color) || glm::isinf(color)))
        return;

    auto currentColor = pixelVar.load();
    while (!pixelVar.compare_exchange_weak(currentColor, currentColor + color))
        ;
}

glm::ivec2 Sensor::getResolution() const
{
    return m_resolution;
}

gsl::span<const glm::vec3> Sensor::getFramebufferRaw()
{
    std::transform(std::begin(m_frameBuffer), std::end(m_frameBuffer), std::begin(m_frameBufferCopy), [](const std::atomic<glm::vec3>& v) -> glm::vec3 { return v.load(); });
    return m_frameBufferCopy;
}

int Sensor::getIndex(int x, int y) const
{
    return y * m_resolution.x + x;
}

}
