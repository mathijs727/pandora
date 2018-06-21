#include "pandora/core/light.h"

namespace pandora {

Light::Light(int flags, const glm::mat4& lightToWorld, int numSamples)
    : m_flags(flags)
    , m_numSamples(std::max(1, numSamples))
    , m_lightToWorld(lightToWorld)
    , m_worldToLight(glm::inverse(lightToWorld))
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

glm::vec3 Light::lightToWorld(const glm::vec3& v) const
{
    return m_lightToWorld * glm::vec4(v, 1);
}

glm::vec3 Light::worldToLight(const glm::vec3& v) const
{
    return m_worldToLight * glm::vec4(v, 1);
}

}
