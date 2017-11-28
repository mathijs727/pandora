#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/intersect_sphere.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"
#include <algorithm>
#include <iostream>

using namespace pandora;
using namespace atlas;

const int width = 1280;
const int height = 720;

int main()
{
    PerspectiveCamera camera = PerspectiveCamera(width, height, 65.0f);
    auto& sensor = camera.getSensor();
    sensor.clear(Vec3f(0.3f, 0.5f, 0.8f));

    Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.2f);

    for (auto [pixel, ray] : camera.generateSamples()) {
        //if (pixel.x > 50 && pixel.x < 1180 && pixel.y > 50 && pixel.y < 620)
        if (intersectSphere(sphere, ray)) {
            sensor.addPixelContribution(pixel, Vec3f(1.0f, 0.0f, 0.0f));
        }
    }

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer;
    frameBuffer.clear(Vec3f(0.8f, 0.5f, 0.3f));
    frameBuffer.update(sensor);
    while (!myWindow.shouldClose()) {
        myWindow.updateInput();
        myWindow.swapBuffers();
    }

    return 0;
}