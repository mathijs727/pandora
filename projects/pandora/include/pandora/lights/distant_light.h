#pragma once
#include "pandora/core/transform.h"
#include "pandora/core/light.h"
#include "pandora/core/texture.h"

namespace pandora {

class DistantLight : public InfiniteLight {
public:
    DistantLight(const glm::mat4& lightToWorld, const Spectrum& L, const glm::vec3& wLight);

    //glm::vec3 power() const final;

    LightSample sampleLi(const Interaction& ref, const glm::vec2& randomSample) const final;
    float pdfLi(const Interaction& ref, const glm::vec3& wi) const final;

private:
    Transform m_transform;
    const Spectrum m_l;
    const glm::vec3 m_wLight;
};

}
