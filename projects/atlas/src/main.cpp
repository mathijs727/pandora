#include "glm/gtc/matrix_transform.hpp"
#include "pandora/core/perspective_camera.h"
#include "pandora/core/scene.h"
#include "pandora/geometry/triangle.h"
#include "pandora/integrators/direct_lighting_integrator.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/integrators/svo_depth_test_integrator.h"
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
#include <chrono>
#include <xmmintrin.h>
#include "pandora/core/load_from_file.h"

using namespace pandora;
using namespace atlas;
using namespace std::string_literals;

const std::string projectBasePath = "../../"s;

RenderConfig createStaticScene();

int main()
{
    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    auto renderConfig = loadFromFile("D:/Pandora Scenes/pbrt_intermediate/buddha-fractal/pandora.json", false);
    //auto renderConfig = createStaticScene();
    Scene& scene = renderConfig.scene;
    PerspectiveCamera& camera = *renderConfig.camera;

    Window myWindow(renderConfig.resolution.x, renderConfig.resolution.y, "Hello World!");
    FramebufferGL frameBuffer(renderConfig.resolution.x, renderConfig.resolution.y);

    FpsCameraControls cameraControls(myWindow, camera);

    /*// Skydome
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    auto transform = glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, Spectrum(1.0f), 1, colorTexture));*/

    //scene.splitLargeSceneObjects(IN_CORE_BATCHING_PRIMS_PER_LEAF);

    //DirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1, LightStrategy::UniformSampleOne);
    //NaiveDirectLightingIntegrator integrator(8, scene, camera.getSensor(), 1);
    //PathIntegrator integrator(10, scene, camera.getSensor(), 1);
    //SVOTestIntegrator integrator(scene, camera.getSensor(), 1);
    SVODepthTestIntegrator integrator(scene, camera.getSensor(), 1);

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

        if (samples % 1 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - previousTimestamp);
            std::cout << "Time time per frame: " << timeDelta.count() / 1.0f / 1000.0f << " miliseconds" << std::endl;
            previousTimestamp = now;
        }

        float mult = 1.0f / integrator.getCurrentFrameSpp();
        frameBuffer.update(camera.getSensor(), mult);
        myWindow.swapBuffers();
    }

    return 0;
}

void addCrytekSponza(Scene& scene);
void addStanfordBunny(Scene& scene);
void addStanfordDragon(Scene& scene, bool loadFromCache = false);
void addCornellBox(Scene& scene);

RenderConfig createStaticScene()
{
    RenderConfig config(0);
    config.resolution = glm::ivec2(1280, 720);
    config.camera = std::make_unique<PerspectiveCamera>(config.resolution, 65.0f);
    //addCornellBox(config.scene);
    //addStanfordBunny(config.scene);
    //addStanfordDragon(config.scene, false);
    addCrytekSponza(config.scene);

    // Sponza
    glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-0.718526125f, 0.0f, 0.263607413f));
    cameraTransform *= glm::mat4_cast(glm::quat(0.182672247f, -0.692262709f, 0.178126544f, 0.675036848f));
    
    //glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.25f, 0.8f, -1.5f)); // Bunny / Dragon

    config.camera->setTransform(cameraTransform);
    return std::move(config);
}

void addCrytekSponza(Scene& scene)
{
    auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.9f, 0.9f, 0.9f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.05f);
    auto material = std::make_shared<MatteMaterial>(kd, roughness);

    auto transform = glm::mat4(1.0f);
    transform = glm::scale(transform, glm::vec3(0.005f));

    static constexpr bool loadAsSingleMesh = true;
    if constexpr (loadAsSingleMesh)
    {
        auto meshOpt = TriangleMesh::loadFromFileSingleMesh(projectBasePath + "assets/3dmodels/sponza-crytek/sponza.obj", transform, false);
        if (meshOpt) {
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(std::make_shared<TriangleMesh>(std::move(*meshOpt)), material));
        }
    } else {
        auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/sponza-crytek/sponza.obj", transform, false);
        for (auto& mesh : meshes) {
            if (mesh.getTriangles().size() < 4)
                continue;

            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(std::make_shared<TriangleMesh>(std::move(mesh)), material));
        }
    }
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

    if constexpr (true) {
        for (auto& mesh : meshes)
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(std::make_shared<TriangleMesh>(std::move(mesh)), material));
    } else {
        assert(meshes.size() == 1);
        const TriangleMesh& bunnyMesh = meshes[0];
        unsigned numPrimitives = bunnyMesh.numTriangles();

        unsigned primsPart1 = 25000;
        std::vector<unsigned> primitives1(primsPart1);
        std::vector<unsigned> primitives2(numPrimitives - primsPart1);
        std::iota(std::begin(primitives1), std::end(primitives1), 0);
        std::iota(std::begin(primitives2), std::end(primitives2), primsPart1);

        std::vector<std::shared_ptr<TriangleMesh>> splitMeshes;
        splitMeshes.emplace_back(std::make_shared<TriangleMesh>(std::move(bunnyMesh.subMesh(primitives1))));
        splitMeshes.emplace_back(std::make_shared<TriangleMesh>(std::move(bunnyMesh.subMesh(primitives2))));

        for (const auto& mesh : splitMeshes)
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(mesh, material));
    }
}

void addStanfordDragon(Scene& scene, bool loadFromCache)
{
    /*auto transform = glm::mat4(1.0f);
    transform = glm::translate(transform, glm::vec3(0, -0.5f, 0));
    transform = glm::scale(transform, glm::vec3(10));
    transform = glm::rotate(transform, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.1f, 0.1f, 0.5f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.05f);
    //auto material = std::make_shared<MatteMaterial>(kd, roughness);
    auto material = MetalMaterial::createCopper(roughness, true);
    if (loadFromCache) {
        auto mesh = std::make_shared<TriangleMesh>(std::move(TriangleMesh::loadFromCacheFile("dragon.geom")));
        scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(mesh, material));
    } else {
        auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/dragon_vrip.ply", transform, false);
        meshes[0].saveToCacheFile("dragon.geom"); // Only a single mesh
        for (auto& mesh : meshes) {
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(std::make_shared<TriangleMesh>(std::move(mesh)), material));
        }
    }*/
}

void addCornellBox(Scene& scene)
{
    auto transform = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, -2.0f, 0.0f)), glm::vec3(0.01f));
    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj", transform);
    //auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.0f);

    for (size_t i = 0; i < meshes.size(); i++) {
        auto& mesh = meshes[i];
        auto meshPtr = std::make_shared<TriangleMesh>(std::move(mesh));

        if (i == 0) {
            // Back box
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(meshPtr, material));
        } else if (i == 1) {
            continue;

            // Front box
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto ks = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.2f, 0.9f));
            auto material = std::make_shared<PlasticMaterial>(kd, ks, roughness);
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(meshPtr, material));
        } else if (i == 5) {
            // Ceiling
            Spectrum light(1.0f);
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(meshPtr, material, light));
        } else {
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.8f, 0.8f, 0.8f));
            auto material = std::make_shared<MatteMaterial>(kd, roughness);
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(meshPtr, material));
        }
    }
}
