#pragma once
#include "pandora/graphics_core/light.h"
#include "pandora/graphics_core/shape.h"
#include "pandora/graphics_core/transform.h"
#include <optional>

namespace pandora {

class AreaLight : public Light {
public:
    AreaLight(glm::vec3 emittedLight);

    glm::vec3 light(const Interaction& ref, const glm::vec3& w) const;

    LightSample sampleLi(const Interaction& ref, PcgRng& rng) const final;
    float pdfLi(const Interaction& ref, const glm::vec3& wi) const final;

private:
    friend class SceneBuilder;

    void attachToShape(const Shape* pShape);
    void attachToShape(const Shape* pShape, const glm::mat4& transform);

private:
    const glm::vec3 m_emmitedLight;
    const Shape* m_pShape { nullptr };
    std::optional<Transform> m_transform;
};

}
