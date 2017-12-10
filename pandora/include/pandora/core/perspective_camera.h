#pragma once
#include "pandora/core/sensor.h"
#include "pandora/math/quaternion.h"
#include "pandora/math/vec2.h"
#include "pandora/traversal/ray.h"
#include "pandora/utility/generator_wrapper.h"
#include <iterator>

namespace pandora {

struct CameraSample {
    CameraSample(Vec2f pixel_)
        : pixel(pixel_)
        , lens()
    {
    }
    Vec2f pixel;
    Vec2f lens;
};

class PerspectiveCamera {
public:
    PerspectiveCamera(float aspectRatio, float fovX);

    Vec3f getPosition() const;
    void setPosition(Vec3f pos);

    QuatF getOrienation() const;
    void setOrientation(QuatF orientation);

    Ray generateRay(const CameraSample& sample) const;

public:
    /*class RayGenIterator : std::iterator<std::forward_iterator_tag, Ray> {
    public:
        RayGenIterator(const PerspectiveCamera& camera, int index);

        std::pair<Vec2i, Ray> operator*() const;

        bool operator==(const RayGenIterator& other);
        bool operator!=(const RayGenIterator& other);

        void operator++();

    private:
        const PerspectiveCamera& m_camera;
        Vec2f m_resolutionFloat;
        Vec2i m_screenCenterPixels;
        Vec2f m_virtualScreenSize; // Size of the camera plane in the world (at distance 1 from the camera)

        int m_currentIndex;
        int m_stride;
    };*/

private:
    Vec2f m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    Vec3f m_position;
    QuatF m_orientation;
};
}