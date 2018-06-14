#pragma once
#include "pandora/traversal/ray.h" // Hit point

namespace pandora {

class Material {
public:
    struct EvalResult {
        glm::vec3 weigth;
        float pdf;
    };
    virtual EvalResult evalBSDF(const IntersectionData& hitPoint, glm::vec3 out) const = 0;

    struct SampleResult {
        glm::vec3 weight;
        float pdf;
        glm::vec3 out;
    };
    virtual SampleResult sampleBSDF(const IntersectionData& hitPoint) const = 0;
};

}
