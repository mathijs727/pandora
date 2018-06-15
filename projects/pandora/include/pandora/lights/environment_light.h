#pragma once
#include "pandora/lights/light.h"
#include "pandora/shading/image_texture.h"

namespace pandora {

class EnvironmentLight : public Light {
public:
    EnvironmentLight(const std::shared_ptr<ImageTexture>& texture);

    glm::vec3 power() const final;

    LightSample sampleLi(const Interaction& ref, const glm::vec2& randomSample) const final;

    glm::vec3 Le(const Ray& ray) const final;

private:
    std::shared_ptr<ImageTexture> m_texture;
};

}
