#pragma once
#include "pandora/core/sensor.h"
#include "pandora/math/quaternion.h"
#include "pandora/math/vec2.h"
#include "pandora/traversal/ray.h"
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

private:
    Vec2f m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    Vec3f m_position;
    QuatF m_orientation;
};
}