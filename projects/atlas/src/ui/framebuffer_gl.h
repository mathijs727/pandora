#pragma once
#include "GL/glew.h"
#include "pandora/core/sensor.h"

using namespace pandora;

namespace atlas {

class FramebufferGL {
public:
    FramebufferGL();
    ~FramebufferGL();

    void clear(glm::vec3 color);
    void update(const Sensor& sensor);

private:
    GLuint m_textureID;
};
}
