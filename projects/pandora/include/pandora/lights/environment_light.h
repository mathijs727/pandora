#pragma once
#include "pandora/core/light.h"
#include "pandora/core/texture.h"

namespace pandora {

class EnvironmentLight : public InfiniteLight {
public:
    EnvironmentLight(const glm::mat4& lightToWorld, const Spectrum& l, int numSamples, const std::shared_ptr<Texture<glm::vec3>>& texture);

    LightSample sampleLi(const Interaction& ref, const glm::vec2& randomSample) const final;
	float pdfLi(const Interaction& ref, const glm::vec3& wi) const final;

    Spectrum Le(const Ray& w) const final;

private:
    glm::vec3 lightToWorld(const glm::vec3& v) const;
    glm::vec3 worldToLight(const glm::vec3& v) const;

private:
    Spectrum m_l;
    std::shared_ptr<Texture<glm::vec3>> m_texture;

    const glm::mat4 m_lightToWorld, m_worldToLight;
};

}
