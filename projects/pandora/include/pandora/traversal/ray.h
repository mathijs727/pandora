#pragma once
#include "glm/glm.hpp"
#include <limits>

namespace pandora {

class TriangleMesh;

struct PathState {
    PathState() = default;
    PathState(glm::ivec2 pixel_)
        : pixel(pixel_)
        , weight(1.0f)
        , depth(0)
    {
    }
    glm::ivec2 pixel;
    glm::vec3 weight;
    int depth;
};

struct Ray {
public:
    Ray() = default;
    Ray(glm::vec3 origin_, glm::vec3 direction_, const PathState& pathState_)
        : origin(origin_)
        , direction(direction_)
        , tnear(0.0f)
        , tfar(std::numeric_limits<float>::max())
        , pathState(pathState_)
    {
    }

    glm::vec3 origin;
    glm::vec3 direction;
    float tnear;
    float tfar;

    // Store this in the ray because otherwise we'd need a ton of extra bookkeeping for memory (de)allocations
    PathState pathState;
};

struct IntersectionData {
    const TriangleMesh* objectHit;
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
