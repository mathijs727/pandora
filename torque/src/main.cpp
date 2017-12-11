#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/intersect_sphere.h"
#include <algorithm>
#include <boost/fiber/all.hpp>
#include <iostream>

using namespace pandora;

const int width = 1280;
const int height = 720;

int main()
{
    float aspectRatio = static_cast<float>(width) / height;
    auto sensor = Sensor(width, height);
    PerspectiveCamera camera = PerspectiveCamera(aspectRatio, 65.0f);

    Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.8f);

    /*boost::fibers::fiber f1([&](int i) {
        auto x = new float[3];
        x[2] = i;
        std::cout << x[1] << std::endl;
        delete[] x;
    }, 123);
    f1.join();*/

    float widthF = static_cast<float>(width);
    float heightF = static_cast<float>(height);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            auto pixelRasterCoords = Vec2i(x, y);
            auto pixelScreenCoords = Vec2f(x / widthF, y / heightF);
            Ray ray = camera.generateRay(CameraSample(pixelScreenCoords));
            ShadeData shadeData = {};
            if (intersectSphere(sphere, ray, shadeData)) {
                // Super basic shading
                Vec3f lightDirection = Vec3f(0.0f, 0.0f, 1.0f).normalized();
                float lightViewCos = dot(shadeData.normal, -lightDirection);

                sensor.addPixelContribution(pixelRasterCoords, Vec3f(1.0f, 0.2f, 0.3f) * lightViewCos);
            }
        }
    }

    return 0;
}