#include "pandora/core/light.h"

namespace pandora {

Light::Light(int flags, int numSamples)
    : m_flags(flags)
    , m_numSamples(std::max(1, numSamples))
{
}

bool Light::isDeltaLight() const
{
    return m_flags & (int)LightFlags::DeltaPosition || m_flags & (int)LightFlags::DeltaDirection;
}

glm::vec3 Light::Le(const glm::vec3& w) const
{
    return glm::vec3(0.0f);
}

}
