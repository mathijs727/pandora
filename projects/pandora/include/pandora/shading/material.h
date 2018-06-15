#pragma once
#include "pandora/core/surface_interaction.h"
#include "pandora/traversal/ray.h"

namespace pandora {

class Material {
public:
    struct EvalResult {
        glm::vec3 weigth;
        float pdf;
    };
    virtual EvalResult evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 wi) const = 0;

    struct SampleResult {
        glm::vec3 weight;
        float pdf;
        glm::vec3 out;
    };
    virtual SampleResult sampleBSDF(const SurfaceInteraction& surfaceInteraction) const = 0;
};

}
