#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/intersect_sphere.h"
#include <iostream>

using namespace pandora;

const int width = 1280;
const int height = 720;

int main()
{
    float aspectRatio = static_cast<float>(width) / height;
    auto sensor = Sensor(width, height);
    PerspectiveCamera camera = PerspectiveCamera(aspectRatio, 65.0f);

    Sphere sphere(glm::vec3(0.0f, 0.0f, 3.0f), 0.8f);

    float widthF = static_cast<float>(width);
    float heightF = static_cast<float>(height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto pixelRasterCoords = glm::ivec2(x, y);
            auto pixelScreenCoords = glm::vec2(x / widthF, y / heightF);
            Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));
            ShadeData shadeData = {};
            if (intersectSphere(sphere, ray, shadeData)) {
                // Super basic shading
                glm::vec3 lightDirection = glm::vec3(0.0f, 0.0f, 1.0f).normalized();
                float lightViewCos = dot(shadeData.normal, -lightDirection);

                sensor.addPixelContribution(pixelRasterCoords, glm::vec3(1.0f, 0.2f, 0.3f) * lightViewCos);
            }
        }
    }

    return 0;
}