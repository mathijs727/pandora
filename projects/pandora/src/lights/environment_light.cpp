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
    : Light((int)LightFlags::Infinite, numSamples)
    , m_l(l)
    , m_texture(texture)
    , m_lightToWorld(lightToWorld)
    , m_worldToLight(glm::inverse(lightToWorld))
{
}

glm::vec3 EnvironmentLight::power() const
{
    // Approximate total power
    float worldRadius = 1.0f; // TODO: Compute bounding sphere of scene?
    return glm::pi<float>() * worldRadius * worldRadius * m_l * m_texture->evaluate(glm::vec2(0.5f, 0.5f));
}

// PBRTv3 page 849
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

// PBRTv3 page 850
float EnvironmentLight::pdfLi(const Interaction& ref, const glm::vec3& wiWorld) const
{
	glm::vec3 wi = worldToLight(wiWorld);
	float theta = sphericalTheta(wi), phi = sphericalTheta(wi);
	float sinTheta = std::sin(theta);
	if (sinTheta == 0)
		return 0.0f;

	return 1.0f / (2 * glm::pi<float>() * glm::pi<float>() * sinTheta);
}

Spectrum EnvironmentLight::Le(const Ray& ray) const
{
    // PBRTv3 page 741
    glm::vec3 w = glm::normalize(worldToLight(ray.direction)); // TODO: worldToLight()
    glm::vec2 st(sphericalPhi(w) * glm::one_over_two_pi<float>(), sphericalTheta(w) * glm::one_over_pi<float>());
    return m_l * m_texture->evaluate(st);
}

glm::vec3 EnvironmentLight::lightToWorld(const glm::vec3& v) const
{
    return m_lightToWorld * glm::vec4(v, 1);
}

glm::vec3 EnvironmentLight::worldToLight(const glm::vec3& v) const
{
    return m_worldToLight * glm::vec4(v, 1);
}

}
