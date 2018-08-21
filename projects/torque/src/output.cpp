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

void linearToOutput(const glm::vec3& linearColor, glm::vec3& output)
{
    glm::vec3 toneMappedOutput = ACESFilm(linearColor);
    glm::vec3 gammaCorrected = glm::pow(toneMappedOutput, glm::vec3(1.0f /2.2f));
    output = gammaCorrected;
}

void writeOutputToFile(pandora::Sensor& sensor, std::string_view fileName)
{
    OIIO::ImageOutput* out = OIIO::ImageOutput::create(fileName.data());
    if (!out) {
        std::cerr << "Could not create output file " << fileName << std::endl;
        return;
    }

    glm::ivec2 resolution = sensor.getResolution();
    auto inPixels = sensor.getFramebufferRaw();
    auto outPixels = std::make_unique<glm::vec3[]>(resolution.x * resolution.y * 3);
    auto extent = resolution.x * resolution.y;
    for (int i = 0; i < extent; i++) {
        linearToOutput(inPixels.get()[extent - i], outPixels[i]);
    }

    OIIO::ImageSpec spec(resolution.x, resolution.y, 3, OIIO::TypeDesc::FLOAT);
    spec.attribute("oiio:ColorSpace", "sRGB");
    out->open(fileName.data(), spec);
    out->write_image(OIIO::TypeDesc::FLOAT, outPixels.get());
    out->close();
    OIIO::ImageOutput::destroy(out);
}
}
