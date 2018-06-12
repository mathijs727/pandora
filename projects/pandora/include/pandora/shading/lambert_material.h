#pragma once
#include "pandora/shading/material.h"

namespace pandora {

class LambertMaterial : public Material {
public:
    LambertMaterial(glm::vec3 color);

    EvalResult evalBSDF(const IntersectionData& hitPoint, glm::vec3 out) override;
    SampleResult sampleBSDF(const IntersectionData& hitPoint) override;

private:
    glm::vec3 m_color;
};

}
