#pragma once
#include "pandora/shading/material.h"
#include "pandora/shading/texture.h"
#include <memory>

namespace pandora {

class LambertMaterial : public Material {
public:
    LambertMaterial(std::shared_ptr<Texture> texture);

    EvalResult evalBSDF(const IntersectionData& hitPoint, glm::vec3 out) const final;
    SampleResult sampleBSDF(const IntersectionData& hitPoint) const final;

private:
    std::shared_ptr<Texture> m_colorTexture;
};

}
