#include "pandora/core/perspective_camera.h"
#include "glm/trigonometric.hpp"
#include "pandora/core/sampler.h"
#include <cmath>
#include <iostream>

namespace pandora {

PerspectiveCamera::PerspectiveCamera(glm::ivec2 resolution, float fovX)
    : m_sensor(resolution)
    , m_resolution(resolution)
    , m_resolutionF(resolution)
    , m_aspectRatio((float)resolution.x / resolution.y)
    , m_fovX(fovX)
    , m_position(0.0f)
    , m_orientation(0.0f, 0.0f, 0.0f, 0.0f)
{
    float virtualScreenWidth = std::tan(glm::radians(m_fovX));
    float virtualScreenHeight = virtualScreenWidth / m_aspectRatio;
    m_virtualScreenSize = glm::vec2(virtualScreenWidth, virtualScreenHeight);
}

glm::vec3 PerspectiveCamera::getPosition() const
{
    return m_position;
}

void PerspectiveCamera::setPosition(glm::vec3 pos)
{
    m_position = pos;
}

glm::quat PerspectiveCamera::getOrienation() const
{
    return m_orientation;
}

void PerspectiveCamera::setOrientation(glm::quat orientation)
{
    m_orientation = orientation;
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

    glm::vec3 origin = m_position;
    glm::vec3 direction = glm::normalize(m_orientation * glm::vec3(pixel2D.x, pixel2D.y, 1.0f));
    return Ray(origin, direction);
}

}
