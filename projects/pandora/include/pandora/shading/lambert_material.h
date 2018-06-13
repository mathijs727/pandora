#pragma once
#include "pandora/shading/material.h"

namespace pandora {

class LambertMaterial : public Material {
public:
    LambertMaterial(glm::vec3 color);

    EvalResult evalBSDF(const IntersectionData& hitPoint, glm::vec3 out) const final;
    SampleResult sampleBSDF(const IntersectionData& hitPoint) const final;

private:
    glm::vec3 m_color;
};

}
