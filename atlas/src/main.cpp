#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/intersect_sphere.h"
#include "ui/fps_camera_controls.h"
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
    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer;
    frameBuffer.clear(Vec3f(0.8f, 0.5f, 0.3f));

    PerspectiveCamera camera = PerspectiveCamera(width, height, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);

    Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.2f);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    while (!myWindow.shouldClose() && !pressedEscape) {
        auto& sensor = camera.getSensor();
        sensor.clear(Vec3f(0.0f));

        myWindow.updateInput();
        cameraControls.tick();

        for (auto [pixel, ray] : camera.generateSamples()) {
            ShadeData shadeData = {};
            if (intersectSphere(sphere, ray, shadeData)) {
                // Super basic shading
                Vec3f lightDirection = Vec3f(0.0f, 0.0f, 1.0f);
                float lightViewCos = dot(shadeData.normal, -lightDirection);

                sensor.addPixelContribution(pixel, Vec3f(1.0f, 0.2f, 0.3f) * lightViewCos);
            }
        }

        frameBuffer.update(sensor);
        myWindow.swapBuffers();
    }

    return 0;
}