#include "glm/gtc/matrix_transform.hpp"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/integrators/direct_lighting_integrator.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/lights/environment_light.h"
#include "pandora/materials/matte_material.h"
#include "pandora/materials/metal_material.h"
#include "pandora/materials/mirror_material.h"
#include "pandora/materials/plastic_material.h"
#include "pandora/materials/translucent_material.h"
#include "pandora/textures/constant_texture.h"
#include "pandora/textures/image_texture.h"
#include "ui/fps_camera_controls.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"

#include <iostream>
#include <pmmintrin.h>
#include <string>
#include <tbb/tbb.h>
#include <xmmintrin.h>

using namespace pandora;
using namespace atlas;
using namespace std::string_literals;

const int width = 1280;
const int height = 720;

const std::string projectBasePath = "../../"s;

void addStanfordBunny(Scene& scene);
void addStanfordDragon(Scene& scene, bool loadFromCache = false);
void addCornellBox(Scene& scene);

int main()
{
    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    Window myWindow(width, height, "Hello World!");
    FramebufferGL frameBuffer(width, height);

    glm::ivec2 resolution = glm::ivec2(width, height);
    PerspectiveCamera camera = PerspectiveCamera(resolution, 65.0f);
    FpsCameraControls cameraControls(myWindow, camera);
    camera.setPosition(glm::vec3(1.5f, 1.5f, 0.0f));

    Scene scene;

	// Skydome
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
	auto transform = glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, Spectrum(0.5f), 1, colorTexture));

	//addCornellBox(scene);
	addStanfordBunny(scene);
	//addStanfordDragon(scene, false);
    
    //DirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1, LightStrategy::UniformSampleOne);
    //NaiveDirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1);
    //PathIntegrator integrator(20, scene, camera.getSensor(), 1);
	SVOTestIntegrator integrator(scene, camera.getSensor(), 1);

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

	auto previousTimestamp = std::chrono::high_resolution_clock::now();
    int samples = 0;
    while (!myWindow.shouldClose() && !pressedEscape) {
        myWindow.updateInput();
        cameraControls.tick();

        if (cameraControls.cameraChanged())
            integrator.startNewFrame();

        integrator.render(camera);
        samples++;

		if (samples % 50 == 0)
		{
			auto now = std::chrono::high_resolution_clock::now();
			auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - previousTimestamp);
			std::cout << "Time time per frame (over last 50 frames): " << timeDelta.count() / 50.0f / 1000.0f << " miliseconds" << std::endl;
			previousTimestamp = now;
		}

        float mult = 1.0f / integrator.getCurrentFrameSpp();
        frameBuffer.update(camera.getSensor(), mult);
        myWindow.swapBuffers();
    }

    return 0;
}

void addStanfordBunny(Scene& scene)
{
	auto transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	transform = glm::scale(transform, glm::vec3(8));
	//transform = glm::translate(transform, glm::vec3(0, -1, 0));
	auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/bun_zipper.ply", transform, false);
	auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.1f, 0.1f, 0.5f));
	auto roughness = std::make_shared<ConstantTexture<float>>(0.05f);
	//auto material = std::make_shared<MatteMaterial>(kd, roughness);
	auto material = MetalMaterial::createCopper(roughness, true);
	for (const auto& mesh : meshes)
		scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
}

void addStanfordDragon(Scene& scene, bool loadFromCache)
{
	auto transform = glm::mat4(1.0f);
	transform = glm::translate(transform, glm::vec3(0, -0.5f, 0));
	transform = glm::scale(transform, glm::vec3(10));
	transform = glm::rotate(transform, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.1f, 0.1f, 0.5f));
	auto roughness = std::make_shared<ConstantTexture<float>>(0.05f);
	//auto material = std::make_shared<MatteMaterial>(kd, roughness);
	auto material = MetalMaterial::createCopper(roughness, true);
	if (loadFromCache) {
		auto mesh = TriangleMesh::loadFromCacheFile("dragon.geom");
		scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
	} else {
		auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/dragon_vrip.ply", transform, false);
		meshes[0]->saveToFile("dragon.geom");// Only a single mesh
		for (const auto& mesh : meshes) {
			scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
		}
	}
}

void addCornellBox(Scene& scene)
{
    auto transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, -2.0f, 0.0f)), glm::vec3(0.01f));
    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj", transform);
    //auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.0f);

    for (size_t i = 0; i < meshes.size(); i++) {
        const auto& mesh = meshes[i];

        if (i == 0) {
            // Back box
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
        } else if (i == 1) {
            continue;

            // Front box
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto ks = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.2f, 0.9f));
            auto material = std::make_shared<PlasticMaterial>(kd, ks, roughness);
            scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
        } else if (i == 5) {
            // Ceiling
            Spectrum light(1.0f);
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<SceneObject>(mesh, material, light));
        } else {
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.8f, 0.8f, 0.8f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<SceneObject>(mesh, material));
        }
    }
}
