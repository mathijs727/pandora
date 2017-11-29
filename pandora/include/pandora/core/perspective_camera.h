#pragma once
#include "pandora/core/sensor.h"
#include "pandora/math/quaternion.h"
#include "pandora/math/vec2.h"
#include "pandora/traversal/ray.h"
#include "pandora/utility/generator_wrapper.h"
#include <iterator>

namespace pandora {

class PerspectiveCamera {
public:
    PerspectiveCamera(int width, int height, float fovX);

    Vec3f getPosition() const;
    void setPosition(Vec3f pos);

    QuatF getOrienation() const;
    void setOrientation(QuatF orientation);

    class RayGenIterator;
    GeneratorWrapper<RayGenIterator> generateSamples();

    Sensor& getSensor();

public:
    class RayGenIterator : std::iterator<std::forward_iterator_tag, Ray> {
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
    };

private:
    Sensor m_sensor;

    Vec2i m_screenSize;
    float m_fovX, m_fovY;

    Vec3f m_position;
    QuatF m_orientation;
};
}