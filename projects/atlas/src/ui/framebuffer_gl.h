#pragma once
#include "pandora/core/sensor.h"
#include <GL/glew.h>

using namespace pandora;

namespace atlas {

class FramebufferGL {
public:
    FramebufferGL();
    ~FramebufferGL();

    void clear(Vec3f color);
    void update(const Sensor& sensor);

private:
    GLuint m_textureID;
};
}