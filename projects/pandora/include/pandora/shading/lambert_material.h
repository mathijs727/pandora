#pragma once
#include "pandora/shading/material.h"
#include "pandora/shading/texture.h"

namespace pandora {

class LambertMaterial : public Material {
public:
    LambertMaterial(std::shared_ptr<Texture> texture);

    EvalResult evalBSDF(const SurfaceInteraction& surfaceInteraction, glm::vec3 out) const final;
    SampleResult sampleBSDF(const SurfaceInteraction& surfaceInteraction) const final;

    glm::vec3 lightEmitted() const final;
private:
    std::shared_ptr<Texture> m_colorTexture;
};

}
