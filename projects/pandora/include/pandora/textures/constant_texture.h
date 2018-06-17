#pragma once
#include "pandora/core/texture.h"

namespace pandora {

class ConstantTexture : public Texture {
public:
    ConstantTexture(glm::vec3 value);
    ~ConstantTexture() = default;

    glm::vec3 evaluate(const glm::vec2& point) const final;
    glm::vec3 evaluate(const SurfaceInteraction& intersection) const final;

private:
    const glm::vec3 m_value;
};

}
