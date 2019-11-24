#include "pandora/graphics_core/perspective_camera.h"
#include "glm/trigonometric.hpp"
#include "pandora/graphics_core/sampler.h"
#include <cmath>
#include <iostream>

namespace pandora {

PerspectiveCamera::PerspectiveCamera(float aspectRatio, float fovX, glm::mat4 transform)
    : PerspectiveCamera(aspectRatio, fovX, 0.3f, 17.0f, transform)
{
}

PerspectiveCamera::PerspectiveCamera(
    float aspectRatio, float fovX, float lensRadius, float focalDistance,
    glm::mat4 transform)
    : m_aspectRatio(aspectRatio)
    , m_fovX(fovX)
    , m_lensRadius(lensRadius)
    , m_focalDistance(focalDistance)
    , m_transform(transform)
{
    updatePosition();
}

glm::mat4 PerspectiveCamera::getTransform() const
{
    return m_transform;
}

void PerspectiveCamera::setTransform(const glm::mat4& matrix)
{
    m_transform = matrix;
    updatePosition();
}

Ray PerspectiveCamera::generateRay(const glm::vec2& sample) const
{
    const glm::vec2 centeredSample = sample - 0.5f;

    const glm::vec3 lensSample = m_lensCenter + centeredSample.x * m_lensDu + centeredSample.y * m_lensDv;
    const glm::vec3 screenSample = m_screenCenter + centeredSample.x * m_screenDu + centeredSample.y * m_screenDv;

    const glm::vec3 origin = lensSample;
    const glm::vec3 direction = glm::normalize(screenSample - lensSample);
    return Ray(origin, direction);
}

void PerspectiveCamera::updatePosition()
{
    // Based on https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/semantic/Camera.cpp
    m_lensCenter = m_transform[3];
    m_lensDu = m_lensRadius * m_transform[0];
    m_lensDv = m_lensRadius * m_transform[1];

    const float fovDistanceToUnitPlane = 0.5f / std::tan(glm::radians(m_fovX / 2.0f));
    m_screenCenter = m_transform[3] + m_focalDistance * m_transform[2];
    m_screenDu = -m_focalDistance / fovDistanceToUnitPlane * m_transform[0];
    m_screenDv = -m_focalDistance / fovDistanceToUnitPlane * m_transform[1];

    /*float virtualScreenWidth = std::tan(glm::radians(m_fovX / 2.0f)) * 2;
    float virtualScreenHeight = virtualScreenWidth / m_aspectRatio;
    m_virtualScreenSize = glm::vec2(virtualScreenWidth, virtualScreenHeight);*/
}

}
