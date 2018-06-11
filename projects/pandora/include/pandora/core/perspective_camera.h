#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "pandora/core/sensor.h"
#include "pandora/traversal/ray.h"
#include <iterator>

namespace pandora {

struct CameraSample {
    CameraSample(glm::vec2 pixel_)
        : pixel(pixel_)
        , lens()
    {
    }
    glm::vec2 pixel;
    glm::vec2 lens;
};

class PerspectiveCamera {
public:
    PerspectiveCamera(float aspectRatio, float fovX);

    glm::vec3 getPosition() const;
    void setPosition(glm::vec3 pos);

    glm::quat getOrienation() const;
    void setOrientation(glm::quat orientation);

    Ray generateRay(const CameraSample& sample) const;

private:
    glm::vec2 m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    glm::vec3 m_position;
    glm::quat m_orientation;
};
}
