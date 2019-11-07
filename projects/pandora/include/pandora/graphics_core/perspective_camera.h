#pragma once
#include "pandora/graphics_core/ray.h"
#include "pandora/graphics_core/sampler.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gsl/span>

namespace pandora {

class PerspectiveCamera {
public:
    PerspectiveCamera(float aspectRatio, float fovX, glm::mat4 transform = glm::mat4(1.0f));

    glm::mat4 getTransform() const;
    void setTransform(const glm::mat4& m);

    Ray generateRay(const glm::vec2& sample) const;

private:
    glm::vec2 m_virtualScreenSize;
    float m_aspectRatio;
    float m_fovX;

    glm::mat4 m_transform;
};
}
