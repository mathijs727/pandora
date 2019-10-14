#pragma once
#include "pandora/graphics_core/light.h"
#include "pandora/graphics_core/shape.h"

namespace pandora {

class AreaLight : public Light {
public:
    AreaLight(glm::vec3 emittedLight, const Shape* pShape);

    glm::vec3 light(const Interaction& ref, const glm::vec3& w) const;

    LightSample sampleLi(const Interaction& ref, PcgRng& rng) const final;
	float pdfLi(const Interaction& ref, const glm::vec3& wi) const final;
private:
    const glm::vec3 m_emmitedLight;
    const Shape* m_pShape;
};

}
