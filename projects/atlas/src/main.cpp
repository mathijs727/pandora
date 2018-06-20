#include "glm/gtc/matrix_transform.hpp"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/integrators/sampler_integrator.h"
#include "pandora/lights/environment_light.h"
#include "pandora/materials/matte_material.h"
#include "pandora/materials/mirror_material.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/textures/image_texture.h"
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
    PerspectiveCamera camera = PerspectiveCamera(resolution, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(glm::vec3(0.0f, 0.5f, -4.0f));

    Scene scene;
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(colorTexture));

    {
        //auto transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.075f));
        auto meshMaterialPairs = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/sphere.obj", transform);
        auto colorTexture = std::make_shared<ConstantTexture<Spectrum>>(glm::vec3(1.0f));
        auto roughnessTexture = std::make_shared<ConstantTexture<float>>(0.0f);
        //auto colorTexture = std::make_shared<ImageTexture>(projectBasePath + "assets/textures/checkerboard.jpg");
        //auto material = std::make_shared<LambertMaterial>(colorTexture);
        auto material = std::make_shared<MatteMaterial>(colorTexture, roughnessTexture);
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

    SamplerIntegrator integrator(8, scene, camera.getSensor(), 4);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    int samples = 0;
    while (!myWindow.shouldClose() && !pressedEscape) {
        myWindow.updateInput();
        cameraControls.tick();

        //if (cameraControls.cameraChanged())
        //    renderer.clear();
        camera.getSensor().clear(glm::vec3(0.0f));

        auto prevFrameEndTime = std::chrono::high_resolution_clock::now();
        integrator.render(camera);
        samples++;
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - prevFrameEndTime);
        prevFrameEndTime = now;
        std::cout << "Time to render frame: " << timeDelta.count() / 1000.0f << " miliseconds" << std::endl;

        frameBuffer.update(camera.getSensor(), 1.0f);
        myWindow.swapBuffers();
    }

    return 0;
}
