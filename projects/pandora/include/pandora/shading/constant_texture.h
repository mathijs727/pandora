#pragma once
#include "pandora/shading/texture.h"

namespace pandora {

class ConstantTexture : public Texture {
public:
    ConstantTexture(glm::vec3 value);
    ~ConstantTexture() = default;

    glm::vec3 evaluate(const SurfaceInteraction& intersection) final;

private:
    const glm::vec3 m_value;
};

}
