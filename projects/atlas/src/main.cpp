#include "glm/gtc/matrix_transform.hpp"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/integrators/direct_lighting_integrator.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/lights/environment_light.h"
#include "pandora/materials/matte_material.h"
#include "pandora/materials/mirror_material.h"
#include "pandora/materials/translucent_material.h"
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

    std::string projectBasePath = "../../";

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer(width, height);

    glm::ivec2 resolution = glm::ivec2(width, height);
    PerspectiveCamera camera = PerspectiveCamera(resolution, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(glm::vec3(0.0f, 0.0f, -4.0f));

    Scene scene;
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    //auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/colors.png");
    //auto colorTexture = std::make_shared<ConstantTexture<Spectrum>>(glm::vec3(1));
    glm::mat4 transform(1.0f);
    transform = glm::rotate(transform, -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, Spectrum(1.0f), 1, colorTexture));

    /*{
        //auto transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        auto transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.075f));
        auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/sphere.obj", transform);
        //auto material = std::make_shared<MirrorMaterial>();
        auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
        auto roughness = std::make_shared<ConstantTexture<float>>(0.0f);
        auto material = std::make_shared<MatteMaterial>(kd, roughness);
        for (const auto& mesh : meshes)
            scene.addSceneObject(SceneObject{ mesh, material });
    }*/

    {
        auto transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, -2.0f, 0.0f)), glm::vec3(0.01f));
        auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj", transform);
        //auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
        auto roughness = std::make_shared<ConstantTexture<float>>(0.0f);

        int i = 0;
        for (const auto& mesh : meshes) {
            if (i == 0) {
                // Back box
                auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
                auto material = std::make_shared<MatteMaterial>(kd, roughness);
                scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
            } else if (i == 1) {
                // Front box
                auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.7f, 0.3f, 0.2f));
				auto material = std::make_shared<MatteMaterial>(kd, roughness);
                scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
            } else if (i == 5) {
                // Ceiling
                Spectrum light(1.0f);
                auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
                auto material = std::make_shared<MatteMaterial>(kd, roughness);
                scene.addSceneObject(std::make_unique<SceneObject>(mesh, material, light));
            } else {
                auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.3f, 0.5f, 0.8f));
                auto material = std::make_shared<MatteMaterial>(kd, roughness);
                scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
            }
            i++;
        }
    }

	//DirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1, LightStrategy::UniformSampleAll);
	//NaiveDirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1);
	PathIntegrator integrator(4, scene, camera.getSensor(), 1);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    int samples = 0;
    while (!myWindow.shouldClose() && !pressedEscape) {
        myWindow.updateInput();
        cameraControls.tick();

        if (cameraControls.cameraChanged())
            integrator.startNewFrame();

        auto prevFrameEndTime = std::chrono::high_resolution_clock::now();
        integrator.render(camera);
        samples++;
        auto now = std::chrono::high_resolution_clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - prevFrameEndTime);
        prevFrameEndTime = now;
        std::cout << "Time to render frame: " << timeDelta.count() / 1000.0f << " miliseconds" << std::endl;

        float mult = 1.0f / integrator.getCurrentFrameSpp();
        frameBuffer.update(camera.getSensor(), mult);
        myWindow.swapBuffers();
    }

    return 0;
}
