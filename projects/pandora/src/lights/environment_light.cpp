#include "pandora/lights/environment_light.h"
#include "glm/gtc/constants.hpp"
#include <algorithm>

static float sphericalTheta(const glm::vec3& v)
{
    return std::acos(std::clamp(v.z, -1.0f, 1.0f));
}

static float sphericalPhi(const glm::vec3& v)
{
    float p = std::atan2(v.y, v.x);
    return (p < 0.0f) ? (p + 2.0f * glm::pi<float>()) : p;
}

namespace pandora {

EnvironmentLight::EnvironmentLight(const std::shared_ptr<Texture>& texture)
    : m_texture(texture)
{
}

glm::vec3 EnvironmentLight::power() const
{
    // Approximate total power
    float worldRadius = 1.0f; // TODO: Compute bounding sphere of scene?
    return glm::pi<float>() * worldRadius * worldRadius * m_texture->evaluate(glm::vec2(0.5f, 0.5f));
}

LightSample EnvironmentLight::sampleLi(const Interaction& ref, const glm::vec2& randomSample) const
{
    // TODO: importance sampling
    float theta = randomSample[1] * glm::pi<float>();
    float phi = randomSample[0] * glm::two_pi<float>();
    float cosTheta = std::cos(theta);
    float sinTheta = std::sin(theta);
    float sinPhi = std::sin(phi);
    float cosPhi = std::cos(phi);

    LightSample result;
    result.radiance = m_texture->evaluate(randomSample);
    result.wi = glm::vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
    result.pdf = sinTheta == 0.0f ? 0.0f : 2 * glm::pi<float>() * glm::pi<float>() * sinTheta;
    result.visibilityRay = computeRayWithEpsilon(ref, result.wi);
    return result;
}


// http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2009%20-%20various.pdf
// Slide 36
glm::vec3 EnvironmentLight::Le(const glm::vec3& w) const
{
    // Convert unit vector to polar coordinates
    float u = 1.0f + std::atan2(w.x, -w.z) / glm::pi<float>();
    float v = std::acos(w.y) / glm::pi<float>();

    // Outputted u is in the range [0, 2], we sample using normalized coordinates [0, 1]
    u /= 2.0f;

    return m_texture->evaluate(glm::vec2(u,v));

    //glm::vec2 textureCoords(sphericalPhi(w) * glm::one_over_two_pi<float>(), sphericalTheta(w) * glm::one_over_two_pi<float>());
}

}
