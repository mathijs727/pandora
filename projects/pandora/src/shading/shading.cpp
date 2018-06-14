#include "shading.h"
#include "glm/gtc/quaternion.hpp"
#include <iostream>
#include <random>

namespace pandora {

glm::vec3 uniformSampleHemisphere(glm::vec3 normal)
{
    std::random_device random;
    std::default_random_engine generator(random());
    std::uniform_real_distribution<float> distribution(0.0f, 1.0f);

    const float u1 = distribution(generator);
    const float u2 = distribution(generator);

    // Uniform sampling
    // http://www.rorydriscoll.com/2009/01/07/better-sampling/
    const float r = std::sqrt(1.0f - u1 * u1);
    const float phi = 2 * pi<float>() * u2;
    glm::vec3 sample = glm::vec3(std::cos(phi) * r, std::sin(phi) * r, u1);

    glm::vec3 tangent;
    if (glm::abs(glm::dot(normal, glm::vec3(0, 0, 1))) > 0.5f)
        tangent = glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
    else
        tangent = glm::normalize(glm::cross(normal, glm::vec3(0, 0, 1)));
    glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

    glm::vec3 orientedSample = sample.x * tangent + sample.y * bitangent + sample.z * normal;
    return glm::normalize(orientedSample);
}

}
