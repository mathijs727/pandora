#include "pandora/lights/environment_light.h"
#include "glm/gtc/constants.hpp"
#include <algorithm>
#include <iostream>

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

EnvironmentLight::EnvironmentLight(const glm::mat4& lightToWorld, const Spectrum& l, int numSamples, const std::shared_ptr<Texture<glm::vec3>>& texture)
    : Light((int)LightFlags::Infinite, lightToWorld, numSamples),
    m_l(l),
    m_texture(texture)
{
}

glm::vec3 EnvironmentLight::power() const
{
    // Approximate total power
    float worldRadius = 1.0f; // TODO: Compute bounding sphere of scene?
    return glm::pi<float>() * worldRadius * worldRadius * m_l * m_texture->evaluate(glm::vec2(0.5f, 0.5f));
}

LightSample EnvironmentLight::sampleLi(const Interaction& ref, const glm::vec2& u) const
{
    // TODO: importance sampling
    float mapPDF = 1.0f;
    glm::vec2 uv = u;

    float theta = uv[1] * glm::pi<float>();
    float phi = uv[0] * 2.0f * glm::pi<float>();
    float cosTheta = std::cos(theta);
    float sinTheta = std::sin(theta);
    float sinPhi = std::sin(phi);
    float cosPhi = std::cos(phi);

    LightSample result;
    result.wi = lightToWorld(glm::vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta));
    result.radiance = m_l * m_texture->evaluate(uv);
    if (sinTheta == 0.0f)
        result.pdf = 0.0f;
    else
        result.pdf = mapPDF / (2 * glm::pi<float>() * glm::pi<float>() * sinTheta);
    result.visibilityRay = computeRayWithEpsilon(ref, result.wi);

    return result;
}

glm::vec3 EnvironmentLight::Le(const glm::vec3& dir) const
{
    /*
    // http://www.cs.uu.nl/docs/vakken/magr/2016-2017/slides/lecture%2009%20-%20various.pdf
    // Slide 36
    
    // Convert unit vector to polar coordinates
    float u = 1.0f + std::atan2(w.x, -w.z) / glm::pi<float>();
    float v = std::acos(w.y) / glm::pi<float>();

    // Outputted u is in the range [0, 2], we sample using normalized coordinates [0, 1]
    u /= 2.0f;

    return m_texture->evaluate(glm::vec2(u,v));
    */

    // PBRTv3 page 741
    glm::vec3 w = glm::normalize(worldToLight(dir)); // TODO: worldToLight()
    glm::vec2 st(sphericalPhi(w) * glm::one_over_two_pi<float>(), sphericalTheta(w) * glm::one_over_pi<float>());
    return m_l * m_texture->evaluate(st);
}

}
