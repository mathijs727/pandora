#include "pandora/shading/constant_texture.h"

namespace pandora {

ConstantTexture::ConstantTexture(glm::vec3 value)
    : m_value(value)
{
}

glm::vec3 ConstantTexture::evaluate(const IntersectionData& intersection)
{
    return m_value;
}

}
