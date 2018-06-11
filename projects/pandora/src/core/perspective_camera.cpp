#include "pandora/core/perspective_camera.h"
#include "glm/trigonometric.hpp"
#include <cmath>
#include <iostream>

namespace pandora {

/*PerspectiveCamera::RayGenIterator::RayGenIterator(const PerspectiveCamera& camera, int index)
    : m_camera(camera)
    , m_resolutionFloat(m_camera.m_screenSize)
    , m_screenCenterPixels(camera.m_screenSize / 2)
    , m_currentIndex(index)
    , m_stride(camera.m_screenSize.x)
{
    float virtualScreenWidth = std::tan(degreesToRadian(camera.m_fovX));
    float virtualScreenHeight = virtualScreenWidth / camera.m_screenSize.x * camera.m_screenSize.y;
    m_virtualScreenSize = glm::vec2(virtualScreenWidth, virtualScreenHeight);
}

std::pair<glm::ivec2, Ray> PerspectiveCamera::RayGenIterator::operator*() const
{
    int x = m_currentIndex % m_stride;
    int y = m_currentIndex / m_stride;

    glm::ivec2 screenPos = glm::ivec2(x, y) - m_screenCenterPixels;
    glm::vec2 pixel2D = (glm::vec2)screenPos / m_resolutionFloat * m_virtualScreenSize;

    glm::vec3 origin = m_camera.m_position;
    glm::vec3 direction = m_camera.m_orientation.rotateVector(glm::vec3(pixel2D.x, pixel2D.y, 1.0f).normalized());

    return { glm::ivec2(x, y), Ray(origin, direction) };
}

bool PerspectiveCamera::RayGenIterator::operator==(const RayGenIterator& other)
{
    return m_currentIndex == other.m_currentIndex;
}

bool PerspectiveCamera::RayGenIterator::operator!=(const RayGenIterator& other)
{
    return m_currentIndex != other.m_currentIndex;
}

void PerspectiveCamera::RayGenIterator::operator++()
{
    m_currentIndex++;
}*/

PerspectiveCamera::PerspectiveCamera(float aspectRatio, float fovX)
    : m_aspectRatio(aspectRatio)
    , m_fovX(fovX)
    , m_position(0.0f)
{
    float virtualScreenWidth = std::tan(glm::radians(m_fovX));
    float virtualScreenHeight = virtualScreenWidth / aspectRatio;
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

Ray PerspectiveCamera::generateRay(const CameraSample& sample) const
{
    glm::vec2 pixel2D = (sample.pixel - 0.5f) * m_virtualScreenSize;

    glm::vec3 origin = m_position;
    glm::vec3 direction = glm::normalize(m_orientation * glm::vec3(pixel2D.x, pixel2D.y, 1.0f));
    return Ray(origin, direction);
}
}
