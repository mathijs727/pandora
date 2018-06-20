#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"

namespace pandora {

// PBRTv3 page 778
inline glm::vec2 concentricSampleDisk(const glm::vec2& u)
{
    // Map uniform random numbers to [-1,1]^2
    glm::vec2 uOffset = 2.0f * u - glm::vec2(1);

    // Handle degeneracy at the origin
    if (uOffset.x == 0.0f && uOffset.y == 0.0f)
        return glm::vec2(0.0f);

    // Apply concentric mapping to point
    float theta, r;
    if (std::abs(uOffset.x) > std::abs(uOffset.y)) {
        r = uOffset.x;
        theta = glm::pi<float>() / 4.0f * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = glm::pi<float>() / 2.0f - glm::pi<float>() / 4.0f * (uOffset.x / uOffset.y);
    }
    return r * glm::vec2(std::cos(theta), std::sin(theta));
}

// PBRTv3 page 775
inline glm::vec3 uniformSampleHemisphere(const glm::vec2& u)
{
    float z = u[0];
    float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
    float phi = 2 * glm::pi<float>() * u[1];
    return glm::vec3(r * std::cos(phi), r * std::sin(phi), z);
}

inline float uniformHemispherePdf()
{
    return glm::one_over_two_pi<float>();
}

// PBRTv3 page 780
inline glm::vec3 cosineSampleHemisphere(const glm::vec2& u)
{
    glm::vec2 d = concentricSampleDisk(u);
    float z = std::sqrt(std::max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
    return glm::vec3(d.x, d.y, z);
}

// PBRTv3 page 780
inline float cosineHemispherePdf(float cosTheta)
{
    return cosTheta * glm::one_over_pi<float>();
}

}
