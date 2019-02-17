#pragma once
#include "glm/glm.hpp"
#include <gsl/gsl>
#include <memory>
#include <vector>
#include <atomic>

namespace pandora {

class Sensor {
public:
    Sensor(glm::ivec2 resolution);

    void clear(glm::vec3 color);
    void addPixelContribution(glm::ivec2 pixel, glm::vec3 value);
    
    glm::vec3 pixelValue(glm::ivec2 pixel) const
    {
        return *reinterpret_cast<const glm::vec3*>(&m_frameBuffer[getIndex(pixel.x, pixel.y)]);
    }

    glm::ivec2 getResolution() const;
    //const FrameBufferConstArrayView getFramebufferView() const;
    gsl::span<const glm::vec3> getFramebufferRaw();

private:
    int getIndex(int x, int y) const;

private:
    glm::ivec2 m_resolution;
    std::vector<std::atomic<glm::vec3>> m_frameBuffer;
    std::vector<glm::vec3> m_frameBufferCopy;
};
}
