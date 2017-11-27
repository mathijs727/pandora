#include "ui/framebuffer_gl.h"
#include "ui/gl_error.h"
#include <iostream>

namespace pandora {

FramebufferGL::FramebufferGL()
{
    glGenTextures(1, &m_textureID);
    check_gl_error();
}

FramebufferGL::~FramebufferGL()
{
    glDeleteTextures(1, &m_textureID);
}

void FramebufferGL::clear(Vec3f color)
{
    glClearColor(color.x, color.y, color.z, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void FramebufferGL::update(const Sensor& sensor)
{
    auto sensorData = sensor.getFramebuffer();

    check_gl_error();

    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, sensorData.extent<0>(), sensorData.extent<1>(), 0, GL_RGB, GL_FLOAT, sensorData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    check_gl_error();

    // I'm sorry for this legacy OpenGL code but modern OpenGL is way too complicated if you just want to draw a textured quad
    glBegin(GL_POLYGON);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(-1.0f, 1.0f, 1.0f);
    glEnd();

    check_gl_error();

    glBindTexture(GL_TEXTURE_2D, 0);
}
}