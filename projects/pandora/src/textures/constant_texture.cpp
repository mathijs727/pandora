#include "pandora/textures/constant_texture.h"

namespace pandora {

ConstantTexture::ConstantTexture(glm::vec3 value)
    : m_value(value)
{
}

glm::vec3 ConstantTexture::evaluate(const glm::vec2& point) const {
    return m_value;
}

glm::vec3 ConstantTexture::evaluate(const SurfaceInteraction& intersection) const
{
    return m_value;
}

}
