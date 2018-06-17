#pragma once
#include "glm/glm.hpp"
#include <gsl/gsl>
#include <memory>
#include <vector>

namespace pandora {

class Sensor {
public:
    Sensor(glm::ivec2 resolution);

    void clear(glm::vec3 color);
    void addPixelContribution(glm::ivec2 pixel, glm::vec3 value);

    glm::ivec2 getResolution() const;
    //const FrameBufferConstArrayView getFramebufferView() const;
    gsl::not_null<const glm::vec3*> getFramebufferRaw() const;

private:
    int getIndex(int x, int y) const;

private:
    glm::ivec2 m_resolution;
    std::unique_ptr<glm::vec3[]> m_frameBuffer;
};
}
