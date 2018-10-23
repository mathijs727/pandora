#include "output.h"
#include "pandora/core/sensor.h"
#include <OpenImageIO/imageio.h>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>

namespace torque {

glm::vec3 tonemappingReinhard(glm::vec3 color)
{
    return color / (color + glm::vec3(1.0));
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
glm::vec3 ACESFilm(glm::vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}


void writeOutputToFile(pandora::Sensor& sensor, int spp, std::string_view fileName, bool applyPostProcessing)
{
    OIIO::ImageOutput* out = OIIO::ImageOutput::create(fileName.data());
    if (!out) {
        std::cerr << "Could not create output file " << fileName << std::endl;
        return;
    }

    glm::ivec2 resolution = sensor.getResolution();
    auto inPixels = sensor.getFramebufferRaw();
    auto outPixels = std::vector<glm::vec3>(resolution.x * resolution.y);
    float invSPP = 1.0f / static_cast<float>(spp);
    if (applyPostProcessing) {
        std::transform(std::begin(inPixels), std::end(inPixels), std::begin(outPixels), [=](const glm::vec3& linear) {
            glm::vec3 toneMappedOutput = ACESFilm(linear * invSPP);
            glm::vec3 gammaCorrected = glm::pow(toneMappedOutput, glm::vec3(1.0f / 2.2f));
            return gammaCorrected;
        });
    } else {
        std::transform(std::begin(inPixels), std::end(inPixels), std::begin(outPixels), [=](const glm::vec3& linear) {
            return linear * invSPP;
        });
    }

    OIIO::ImageSpec spec(resolution.x, resolution.y, 3, OIIO::TypeDesc::FLOAT);
    if (applyPostProcessing)
        spec.attribute("oiio:ColorSpace", "srgb");
    else
        spec.attribute("oiio:ColorSpace", "linear");
    out->open(fileName.data(), spec);
    out->write_image(OIIO::TypeDesc::FLOAT, outPixels.data());
    out->close();
    OIIO::ImageOutput::destroy(out);
}
}
