#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/traversal/intersect_sphere.h"
#include "tbb/tbb.h"
#include "ui/fps_camera_controls.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"
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

    float aspectRatio = static_cast<float>(width) / height;
    PerspectiveCamera camera = PerspectiveCamera(aspectRatio, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    //camera.setPosition(Vec3f(0.0f, 0.0f, 3.0f));
    //camera.setOrientation(Vec3f(0.0f, 0.0f, -1.0f).normalized(), Vec3f(0.0f, 1.0f, 0.0f));
    auto sensor = Sensor(width, height);

    Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.8f);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    while (!myWindow.shouldClose() && !pressedEscape) {
        sensor.clear(Vec3f(0.0f));

        myWindow.updateInput();
        cameraControls.tick();

        float widthF = static_cast<float>(width);
        float heightF = static_cast<float>(height);

        tbb::parallel_for(0, height, [&](int y) {
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
        });

        frameBuffer.update(sensor);
        myWindow.swapBuffers();
    }

    return 0;
}