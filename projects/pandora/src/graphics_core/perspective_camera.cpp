#include "pandora/graphics_core/perspective_camera.h"
#include "glm/trigonometric.hpp"
#include "pandora/graphics_core/sampler.h"
#include <cmath>
#include <iostream>

namespace pandora {

PerspectiveCamera::PerspectiveCamera(glm::ivec2 resolution, float fovX, glm::mat4 transform)
    : m_sensor(resolution)
    , m_resolution(resolution)
    , m_resolutionF(resolution)
    , m_aspectRatio((float)resolution.x / resolution.y)
    , m_fovX(fovX)
    , m_transform(transform)
{
    float virtualScreenWidth = std::tan(glm::radians(m_fovX / 2.0f)) * 2;
    float virtualScreenHeight = virtualScreenWidth / m_aspectRatio;
    m_virtualScreenSize = glm::vec2(virtualScreenWidth, virtualScreenHeight);
}

glm::mat4 PerspectiveCamera::getTransform() const
{
    return m_transform;
}

void PerspectiveCamera::setTransform(const glm::mat4& matrix)
{
    m_transform = matrix;
}

Sensor& PerspectiveCamera::getSensor()
{
    return m_sensor;
}

glm::ivec2 PerspectiveCamera::getResolution() const
{
    return m_resolution;
}

Ray PerspectiveCamera::generateRay(const CameraSample& sample) const
{
    glm::vec2 pixel2D = (sample.pixel / m_resolutionF - 0.5f) * m_virtualScreenSize;

    glm::vec3 origin = m_transform * glm::vec4(0, 0, 0, 1);
    glm::vec3 direction = glm::normalize(m_transform * glm::vec4(pixel2D.x, pixel2D.y, 1.0f, 0.0f));
    return Ray(origin, direction);
}

}
