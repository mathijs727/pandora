#pragma once
#include "glm/glm.hpp"
//#include <boost/multi_array.hpp>
#include <gsl/gsl>
#include <memory>
#include <vector>

namespace pandora {

class Sensor {
public:
    Sensor(int width, int height);

    void clear(glm::vec3 color);
    void addPixelContribution(glm::ivec2 pixel, glm::vec3 value);

    int width() const { return m_width; }
    int height() const { return m_height; }
    //const FrameBufferConstArrayView getFramebufferView() const;
    gsl::not_null<const glm::vec3*> getFramebufferRaw() const;
private:
    int getIndex(int x, int y) const;

private:
    int m_width, m_height;
    std::unique_ptr<glm::vec3[]> m_frameBuffer;
};
}
