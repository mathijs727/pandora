#include "pandora/core/perspective_camera.h"
#include "pandora/math/constants.h"
#include "pandora/math/math_functions.h"
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
    m_virtualScreenSize = Vec2f(virtualScreenWidth, virtualScreenHeight);
}

std::pair<Vec2i, Ray> PerspectiveCamera::RayGenIterator::operator*() const
{
    int x = m_currentIndex % m_stride;
    int y = m_currentIndex / m_stride;

    Vec2i screenPos = Vec2i(x, y) - m_screenCenterPixels;
    Vec2f pixel2D = (Vec2f)screenPos / m_resolutionFloat * m_virtualScreenSize;

    Vec3f origin = m_camera.m_position;
    Vec3f direction = m_camera.m_orientation.rotateVector(Vec3f(pixel2D.x, pixel2D.y, 1.0f).normalized());

    return { Vec2i(x, y), Ray(origin, direction) };
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
    float virtualScreenWidth = std::tan(degreesToRadian(m_fovX));
    float virtualScreenHeight = virtualScreenWidth / aspectRatio;
    m_virtualScreenSize = Vec2f(virtualScreenWidth, virtualScreenHeight);
}

Vec3f PerspectiveCamera::getPosition() const
{
    return m_position;
}

void PerspectiveCamera::setPosition(Vec3f pos)
{
    m_position = pos;
}

QuatF PerspectiveCamera::getOrienation() const
{
    return m_orientation;
}

void PerspectiveCamera::setOrientation(QuatF orientation)
{
    m_orientation = orientation;
}

Ray PerspectiveCamera::generateRay(const CameraSample& sample) const
{
    Vec2f pixel2D = (sample.pixel - 0.5f) * m_virtualScreenSize;

    Vec3f origin = m_position;
    Vec3f direction = m_orientation.rotateVector(Vec3f(pixel2D.x, pixel2D.y, 1.0f).normalized());
    return Ray(origin, direction);
}
}