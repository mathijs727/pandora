#pragma once
#include "glm/glm.hpp"
#include <limits>

namespace pandora {

struct SceneObject;

struct Ray {
public:
    Ray() = default;
    Ray(glm::vec3 origin_, glm::vec3 direction_)
        : origin(origin_)
        , direction(direction_)
        , tnear(0.0f)
        , tfar(std::numeric_limits<float>::max())
    {
    }

    glm::vec3 origin;
    glm::vec3 direction;
    float tnear;
    float tfar;
};

struct IntersectionData {
    const SceneObject* sceneObject;
    unsigned primitiveID;

    glm::vec3 position;
    glm::vec3 incident;

    glm::vec3 geometricNormal;

    glm::vec2 uv;

    /*// Surface position;
    glm::vec3 position, dPdx, dPdy;

    // Incident ray, and its x and y derivatives
    glm::vec3 incident, dIdx, dIdy;

    // Shading normal and geometric normal
    glm::vec3 shadingNormal;
    glm::vec3 geometricNormal;

    // Surface parameters uv and their differentials
    glm::vec2 uv, dUVdx, dUVdy;

    // Surface tangents: derivative of position with respect to surface u and v
    glm::vec3 dPdu, dPdv;*/
};

}
