#include "glm/gtc/matrix_transform.hpp"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/progressive_renderer.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/lights/environment_light.h"
#include "pandora/shading/constant_texture.h"
#include "pandora/shading/image_texture.h"
#include "pandora/shading/lambert_material.h"
#include "pandora/shading/mirror_material.h"
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

    Scene scene;
    auto colorTexture = std::make_shared<ImageTexture>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(colorTexture));

    {
        //auto transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.075f));
        auto meshMaterialPairs = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/sphere.obj", transform);
        auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
        //auto colorTexture = std::make_shared<ImageTexture>(projectBasePath + "assets/textures/checkerboard.jpg");
        //auto material = std::make_shared<LambertMaterial>(colorTexture);
        auto material = std::make_shared<MirrorMaterial>();
        for (auto [mesh, _] : meshMaterialPairs)
            scene.addSceneObject(SceneObject{ mesh, material });
    }

    /*{
        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
        auto meshMaterialPairs = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj", transform);
        //auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
        auto colorTexture = std::make_shared<ImageTexture>(projectBasePath + "assets/textures/checkerboard.jpg");
        auto material = std::make_shared<LambertMaterial>(colorTexture);
        for (auto [mesh, _] : meshMaterialPairs)
            scene.addSceneObject(SceneObject{ mesh, material });
    }*/

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
