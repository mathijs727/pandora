#include "pandora/core/perspective_camera.h"
#include "pandora/geometry/sphere.h"
#include "pandora/geometry/triangle.h"
#include "pandora/math/constants.h"
#include "pandora/traversal/intersect_sphere.h"
#include "pandora/traversal/sbvh.h"
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
#ifdef WIN32
    std::string projectBasePath = "C:/Users/Mathijs/Documents/GitHub/pandora/";
#else
    std::string projectBasePath = "../";
#endif

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer;
    frameBuffer.clear(Vec3f(0.8f, 0.5f, 0.3f));

    float aspectRatio = static_cast<float>(width) / height;
    PerspectiveCamera camera = PerspectiveCamera(aspectRatio, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(Vec3f(0.0f, 1.0f, -4.0f));
    //camera.setOrientation(QuatF::rotation(Vec3f(0, 1, 0), piF * 1.0f));
    auto sensor = Sensor(width, height);

    //Sphere sphere(Vec3f(0.0f, 0.0f, 3.0f), 0.8f);
    //auto mesh = TriangleMesh::singleTriangle();
    auto mesh = TriangleMesh::loadFromFile(projectBasePath + "assets/CornellBox-Empty-White.obj");
    if (mesh == nullptr) {
#ifdef WIN32
        system("PAUSE");
#endif
        exit(1);
    }

    std::vector<const Shape*> shapes = { mesh.get() };
    TwoLevelSbvhAccel accelerationStructure(shapes);

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

                //for (unsigned i = 0; i < mesh->numPrimitives(); i++) {
                //    mesh->intersect(i, ray);
                //}
                accelerationStructure.intersect(ray);

                if (ray.t < std::numeric_limits<float>::max()) {
                    //sensor.addPixelContribution(pixelRasterCoords, Vec3f(1.0f, 0.2f, 0.3f));
                    sensor.addPixelContribution(pixelRasterCoords, Vec3f(0.0f, ray.uv.x, ray.uv.y));
                }
            }
        });

        frameBuffer.update(sensor);
        myWindow.swapBuffers();
    }

    return 0;
}