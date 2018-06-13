#include "pandora/core/perspective_camera.h"
#include "pandora/core/progressive_renderer.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "ui/fps_camera_controls.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"

#include "tbb/tbb.h"
#include <iostream>
#include <pmmintrin.h>
#include <xmmintrin.h>

using namespace pandora;
using namespace atlas;

const int width = 1280;
const int height = 720;

int main()
{
    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

#ifdef WIN32
    std::string projectBasePath = "C:/Users/Mathijs/Documents/GitHub/pandora/";
#else
    std::string projectBasePath = "../";
#endif

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer(width, height);

    glm::ivec2 resolution = glm::ivec2(width, height);
    Sensor sensor = Sensor(resolution);
    PerspectiveCamera camera = PerspectiveCamera(resolution, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(glm::vec3(0.0f, 0.5f, -4.0f));
    //camera.setOrientation(glm::quat::rotation(glm::vec3(0, 1, 0), piF * 1.0f));

    //auto mesh = TriangleMesh::singleTriangle();
    auto mesh = TriangleMesh::loadFromFile(projectBasePath + "assets/monkey.obj");
    //auto mesh = TriangleMesh::loadFromFile(projectBasePath + "assets/cornell_box.obj");
    if (mesh == nullptr) {
#ifdef WIN32
        system("PAUSE");
#endif
        exit(1);
    }

    Scene scene;
    scene.addMesh(mesh);
    ProgressiveRenderer renderer(scene, sensor);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    while (!myWindow.shouldClose() && !pressedEscape) {
        myWindow.updateInput();
        cameraControls.tick();

        if (cameraControls.cameraChanged())
            renderer.clear();

        auto prevFrameEndTime = std::chrono::high_resolution_clock::now();
        renderer.incrementalRender(camera);
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - prevFrameEndTime);
        prevFrameEndTime = now;
        std::cout << "Time to render frame: " << timeDelta.count() / 1000.0f << " miliseconds" << std::endl;

        frameBuffer.update(sensor, 1.0f / renderer.getSampleCount());
        myWindow.swapBuffers();
    }

    return 0;
}
