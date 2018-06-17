#pragma once
#include "pandora/core/light.h"
#include "pandora/core/texture.h"

namespace pandora {

class EnvironmentLight : public Light {
public:
    EnvironmentLight(const std::shared_ptr<Texture>& texture);

    glm::vec3 power() const final;

    LightSample sampleLi(const Interaction& ref, const glm::vec2& randomSample) const final;

    glm::vec3 Le(const glm::vec3& w) const final;

private:
    std::shared_ptr<Texture> m_texture;
};

}
