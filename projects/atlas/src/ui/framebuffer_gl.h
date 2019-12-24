#pragma once
#include "GL/glew.h"
#include "pandora/graphics_core/sensor.h"

using namespace pandora;

namespace atlas {

class FramebufferGL {
public:
    FramebufferGL(int width, int height);
    ~FramebufferGL();

    void update(Sensor& sensor, float multiplier);

private:
    GLuint loadShader(std::string_view source, GLenum type);

private:
    GLuint m_texture;
    GLuint m_vbo, m_vao;
    GLuint m_shader;
};
}
