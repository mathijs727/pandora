#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "pandora/core/sensor.h"
#include "pandora/traversal/ray.h"
#include <iterator>

namespace pandora {

struct CameraSample;

class PerspectiveCamera {
public:
    PerspectiveCamera(glm::ivec2 resolution, float fovX);

    glm::vec3 getPosition() const;
    void setPosition(glm::vec3 pos);

    glm::quat getOrienation() const;
    void setOrientation(glm::quat orientation);

    Sensor& getSensor();
    glm::ivec2 getResolution() const;

    Ray generateRay(const CameraSample& sample) const;

private:
    Sensor m_sensor;
    glm::ivec2 m_resolution;
    glm::vec2 m_resolutionF;

    glm::vec2 m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    glm::vec3 m_position;
    glm::quat m_orientation;
};
}
