#include "pandora/graphics_core/sensor.h"
#include "pandora/utility/error_handling.h"
#include <iterator>
#include <memory>

static_assert(sizeof(cnl::fixed_point<uint64_t>) == sizeof(uint64_t));

namespace pandora {

Sensor::Sensor(glm::ivec2 resolution)
    : m_resolution(resolution)
    , m_frameBuffer(static_cast<size_t>(resolution.x) * resolution.y)
{
    for (size_t i = 0; i < m_frameBuffer.size(); i++) {
        Pixel defaultPixel { glm::vec3(0.0f) };
        m_frameBuffer[i].store(defaultPixel);
    }

    clear(glm::vec3(0.0f));
    cnl::fixed_point<uint64_t, -24> x;
    x += 2.0f;
}

void Sensor::clear(glm::vec3 color)
{
    const Pixel clearValue { color };
    std::for_each(
        std::begin(m_frameBuffer),
        std::end(m_frameBuffer),
        [=](auto& atomicPixelColor) {
            atomicPixelColor.store(clearValue);
        });
}

void Sensor::addPixelContribution(glm::ivec2 pixel, glm::vec3 color)
{
    auto& pixelVar = m_frameBuffer[getIndex(pixel.x, pixel.y)];

    //ALWAYS_ASSERT(!glm::any(glm::isnan(color) || glm::isinf(color)));
    if (glm::any(glm::isnan(color) || glm::isinf(color)))
        return;

    Pixel fixedPointColor { color };
    auto currentColor = pixelVar.load();
    auto newColor = currentColor + fixedPointColor;
    while (!pixelVar.compare_exchange_weak(currentColor, newColor))
        newColor = currentColor + fixedPointColor;
}

glm::vec3 Sensor::getPixelValue(glm::ivec2 pixel) const
{
    auto p = m_frameBuffer[getIndex(pixel.x, pixel.y)].load();
    return static_cast<glm::vec3>(p);
}

glm::ivec2 Sensor::getResolution() const
{
    return m_resolution;
}

const std::vector<glm::vec3> Sensor::copyFrameBufferVec3() const
{
    std::vector<glm::vec3> out;
    out.resize(m_frameBuffer.size());
    std::transform(
        std::begin(m_frameBuffer),
        std::end(m_frameBuffer),
        std::begin(out),
        [](const std::atomic<Pixel>& v) {
            return static_cast<glm::vec3>(v.load());
        });
    return out;
}

int Sensor::getIndex(int x, int y) const
{
    return y * m_resolution.x + x;
}

Sensor::Pixel::Pixel(const glm::vec3& color)
    : red(static_cast<float>(color.r))
    , green(static_cast<float>(color.g))
    , blue(static_cast<float>(color.b))
{
}

Sensor::Pixel::operator glm::vec3() const
{
    return { static_cast<float>(red), static_cast<float>(green), static_cast<float>(blue) };
}

Sensor::Pixel Sensor::Pixel::operator+(const Sensor::Pixel& other) const
{
    Pixel out;
    out.red = red + other.red;
    out.green = green + other.green;
    out.blue = blue + other.blue;
    return out;
}

}
