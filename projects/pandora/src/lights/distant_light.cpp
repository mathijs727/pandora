#include "pandora/lights/distant_light.h"
#include <glm/gtc/constants.hpp>

// https://github.com/mmp/pbrt-v3/blob/master/src/lights/distant.cpp
// Page 731 of PBRTv3

namespace pandora {

DistantLight::DistantLight(const glm::mat4& lightToWorld, const Spectrum& L, const glm::vec3& wLight)
    : InfiniteLight((int)LightFlags::DeltaDirection)
    , m_transform(lightToWorld)
    , m_l(L)
    , m_wLight(wLight)
{
}

/*glm::vec3 DistantLight::power() const
{
    return m_l * glm::pi<float>();
}*/

LightSample DistantLight::sampleLi(const Interaction& ref, const glm::vec2& randomSample) const
{
    LightSample ret;
    ret.wi = -m_wLight;
    ret.pdf = 1.0f;
    ret.radiance = m_l;
    ret.visibilityRay = ref.spawnRay(-m_wLight);
    return ret;
}

float DistantLight::pdfLi(const Interaction& ref, const glm::vec3& wi) const
{
    return 0.0f;
}

}
