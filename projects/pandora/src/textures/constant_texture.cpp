#include "pandora/textures/constant_texture.h"

namespace pandora {

template <class T>
ConstantTexture<T>::ConstantTexture(const T& value)
    : m_value(value)
{
}

template <class T>
T ConstantTexture<T>::evaluate(const glm::vec2& point) const {
    return m_value;
}

template <class T>
T ConstantTexture<T>::evaluate(const SurfaceInteraction& intersection) const
{
    return m_value;
}

// Explicit instantiation
template class ConstantTexture<float>;
template class ConstantTexture<glm::vec3>;
}

