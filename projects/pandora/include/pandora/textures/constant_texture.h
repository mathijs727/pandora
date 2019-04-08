#pragma once
#include "pandora/graphics_core/texture.h"

namespace pandora {

template <class T>
class ConstantTexture : public Texture<T> {
public:
    ConstantTexture(const T& value);
    ~ConstantTexture() = default;

    T evaluate(const glm::vec2& point) const final;
    T evaluate(const SurfaceInteraction& intersection) const final;

private:
    const T m_value;
};

}
