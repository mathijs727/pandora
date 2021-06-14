#pragma once
#include "pandora/graphics_core/ray.h"
#include "pandora/graphics_core/sampler.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <span>

namespace pandora {

class PerspectiveCamera {
public:
    PerspectiveCamera(float aspectRatio, float fovX, glm::mat4 transform = glm::mat4(1.0f));
    PerspectiveCamera(float aspectRatio, float fovX, float lensRadius, float focalDistance, glm::mat4 transform = glm::mat4(1.0f));

    glm::mat4 getTransform() const;
    void setTransform(const glm::mat4& m);

    Ray generateRay(const glm::vec2& sample) const;

private:
    void updatePosition();

private:
    //glm::vec2 m_virtualScreenSize;
    const float m_aspectRatio;
    const float m_fovX;
    const float m_lensRadius;
    const float m_focalDistance;

    glm::vec3 m_lensCenter;
    glm::vec3 m_lensDu;
    glm::vec3 m_lensDv;
    glm::vec3 m_screenCenter;
    glm::vec3 m_screenDu;
    glm::vec3 m_screenDv;

    glm::mat4 m_transform;
};
}
