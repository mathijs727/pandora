#include "ui/framebuffer_gl.h"
#include "ui/gl_error.h"
#include <iostream>

namespace atlas {

FramebufferGL::FramebufferGL()
{
    glGenTextures(1, &m_textureID);
    check_gl_error();
}

FramebufferGL::~FramebufferGL()
{
    glDeleteTextures(1, &m_textureID);
}

void FramebufferGL::clear(glm::vec3 color)
{
    glClearColor(color.x, color.y, color.z, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void FramebufferGL::update(const Sensor& sensor)
{
    // NOTE: the interal data structure of boost::multi_array orders has the rows/columns swapped when
    // compared to what OpenGL expects. So we create a rotated texture as use the texture coordinates to
    // rotate it back when drawing it to the screen.
    check_gl_error();

    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sensor.height(), sensor.width(),
        0, GL_RGB, GL_FLOAT, sensor.getFramebufferRaw());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    check_gl_error();

    // I'm sorry for this legacy OpenGL code but modern OpenGL is way too complicated if you just
    // want to draw a textured quad.
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glEnd();

    check_gl_error();

    glBindTexture(GL_TEXTURE_2D, 0);
}
}