#pragma once
#include "glm/glm.hpp"
#include "pandora/core/surface_interaction.h"
#include "pandora/traversal/ray.h"
#include <optional>

namespace pandora {

struct LightSample {
    glm::vec3 radiance;

    glm::vec3 wi;
    float pdf;

    Ray visibilityRay;
};

class Light {
public:
    virtual glm::vec3 power() const = 0;
    virtual LightSample sampleLi(const Interaction& interaction, const glm::vec2& randomSample) const = 0; // Sample_Li

    virtual glm::vec3 Le(const glm::vec3& w) const; // Radiance added to rays that miss the scene
};

}
