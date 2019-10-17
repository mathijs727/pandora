#include "pandora/config.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/load_from_file.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/normal_debug_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/materials/matte_material.h"
#include "pandora/shapes/triangle.h"
#include "pandora/textures/constant_texture.h"
#include "stream/task_graph.h"
#include "ui/fps_camera_controls.h"
#include "ui/framebuffer_gl.h"
#include "ui/window.h"

#include "pandora/graphics_core/load_from_file.h"
#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <spdlog/spdlog.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>

using namespace pandora;
using namespace atlas;
using namespace std::string_literals;

const std::string projectBasePath = "../../"s;

RenderConfig createStaticScene();

int main(int argc, char** argv)
{
#ifdef _WIN32
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);
#else
    auto colorLogger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("color_logger");
    spdlog::set_default_logger(colorLogger);
#endif

    spdlog::info("Parsing input");

    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    namespace po = boost::program_options;
    po::options_description desc("Pandora options");
    // clang-format off
    desc.add_options()
		("file", po::value<std::string>()->required(), "Pandora scene description JSON")
		("out", po::value<std::string>()->default_value("output"s), "output name (without file extension!)")
		("integrator", po::value<std::string>()->default_value("direct"), "integrator (normal, direct or path)")
		("spp", po::value<int>()->default_value(1), "samples per pixel")
		("help", "show all arguments");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    try {
        po::notify(vm);
    } catch (const boost::program_options::required_option& e) {
        std::cout << "Missing required argument \"" << e.get_option_name() << "\"" << std::endl;
        return 1;
    }

    std::cout << "Rendering with the following settings:\n";
    std::cout << "  file          " << vm["file"].as<std::string>() << "\n";
    std::cout << "  out           " << vm["out"].as<std::string>() << "\n";
    std::cout << "  integrator    " << vm["integrator"].as<std::string>() << "\n";
    std::cout << "  spp           " << vm["spp"].as<int>() << std::endl;

    g_stats.config.sceneFile = vm["file"].as<std::string>();
    g_stats.config.integrator = vm["integrator"].as<std::string>();
    g_stats.config.spp = vm["spp"].as<int>();

    spdlog::info("Loading scene");
    RenderConfig renderConfig = loadFromFile(vm["file"].as<std::string>());
    const glm::ivec2 resolution = renderConfig.resolution;

    Window myWindow(resolution.x, resolution.y, "Atlas - Pandora viewer");
    FramebufferGL frameBuffer(resolution.x, resolution.y);
    FpsCameraControls cameraControls(myWindow, *renderConfig.camera);

    spdlog::info("Creating integrator");
    tasking::TaskGraph taskGraph;
    const int spp = vm["spp"].as<int>();
    NormalDebugIntegrator integrator { &taskGraph };
    //DirectLightingIntegrator integrator { &taskGraph, 8, spp, LightStrategy::UniformSampleOne };
    //PathIntegrator integrator { &taskGraph, 8, spp, LightStrategy::UniformSampleOne };

    spdlog::info("Building acceleration structure");
    EmbreeAccelerationStructureBuilder accelBuilder { *renderConfig.pScene, &taskGraph };
    auto accel = accelBuilder.build(integrator.hitTaskHandle(), integrator.missTaskHandle(), integrator.anyHitTaskHandle(), integrator.anyMissTaskHandle());

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    auto& camera = *renderConfig.camera;
    auto& sensor = renderConfig.camera->getSensor();
    auto previousTimestamp = std::chrono::high_resolution_clock::now();
    int samples = 0;
    while (!myWindow.shouldClose() && !pressedEscape) {
        myWindow.updateInput();
        cameraControls.tick();

        if (cameraControls.cameraChanged()) {
            samples = 0;
            sensor.clear(glm::vec3(0.0f));
        }

#if 0
        integrator.render(camera, sensor, *renderConfig.pScene, accel);
#else
        samples = 0;
        sensor.clear(glm::vec3(0));

        auto& sensor = renderConfig.camera->getSensor();
        tbb::blocked_range2d range { 0, renderConfig.resolution.x, 0, renderConfig.resolution.y };
        tbb::parallel_for(range, [&](tbb::blocked_range2d<int> subRange) {
            for (int y = subRange.cols().begin(); y < subRange.cols().end(); y++) {
                for (int x = subRange.rows().begin(); x < subRange.rows().end(); x++) {
                    CameraSample cameraSample { glm::vec2 { x, y } };
                    Ray cameraRay = renderConfig.camera->generateRay(cameraSample);
                    auto hitOpt = accel.intersectFast(cameraRay);
                    if (hitOpt) {
                        //auto si = hitOpt->pSceneObject->pShape->getShadingGeometry()->fillSurfaceInteraction(cameraRay, *hitOpt);
                        //float cos = glm::dot(si.shading.normal, -cameraRay.direction);
                        float cos = glm::dot(hitOpt->geometricNormal, -cameraRay.direction);
                        sensor.addPixelContribution(glm::ivec2 { x, y }, glm::vec3(cos));
                    }
                }
            }
        });
#endif
        samples += spp;

        glm::vec3 camPos = camera.getTransform() * glm::vec4(0, 0, 0, 1);
        std::cout << "camera pos: [" << camPos.x << ", " << camPos.y << ", " << camPos.z << "]" << std::endl;

        if ((samples / spp) % 1 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(now - previousTimestamp);
            std::cout << "Time time per frame: " << timeDelta.count() / 1.0f / 1000.0f << " miliseconds" << std::endl;
            previousTimestamp = now;
        }

        float mult = 1.0f / samples;
        frameBuffer.update(sensor, mult);
        myWindow.swapBuffers();
    }

    return 0;
}

/*void addCrytekSponza(Scene& scene);
void addStanfordBunny(Scene& scene);
void addStanfordDragon(Scene& scene, bool loadFromCache = false);
void addCornellBox(Scene& scene);

RenderConfig createStaticScene()
{
    RenderConfig config(0);
    config.resolution = glm::ivec2(1280, 720);
    config.camera = std::make_unique<PerspectiveCamera>(config.resolution, 65.0f);
    //addCornellBox(config.scene);
    addStanfordBunny(config.scene);
    //addStanfordDragon(config.scene, false);
    //addCrytekSponza(config.scene);

    // Sponza
    //glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(-0.718526125f, 0.0f, 0.263607413f));
    //cameraTransform *= glm::mat4_cast(glm::quat(0.182672247f, -0.692262709f, 0.178126544f, 0.675036848f));

    glm::mat4 cameraTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.25f, 0.8f, -1.5f)); // Bunny / Dragon
    cameraTransform = glm::scale(cameraTransform, glm::vec3(1, -1, 1));
    //glm::mat4 cameraTransform = glm::mat4(1.0f);

    config.camera->setTransform(cameraTransform);

    auto lightTexture = std::make_shared<ImageTexture<Spectrum>>("C:/Users/Mathijs/Documents/GitHub/OpenCL-Path-Tracer/assets/skydome/DF360_005_Ref.hdr");
    config.scene.addInfiniteLight(std::make_shared<EnvironmentLight>(glm::mat4(1.0f), Spectrum(1.0f), 1, lightTexture));
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
    if constexpr (loadAsSingleMesh) {
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

    auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.1f, 0.2f, 0.4f));
    auto ks = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.006f);
    auto t = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(1.0f));
    auto r = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.3f, 0.3f, 0.5f));
    //auto material = std::make_shared<MatteMaterial>(kd, roughness);
    auto material = std::make_shared<TranslucentMaterial>(kd, ks, roughness, r, t, true);

    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/bun_zipper.ply", transform, false);
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
    }/
}

void addCornellBox(Scene& scene)
{
    //auto transform = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(0.01f, 0.01f, -0.01f)), glm::vec3(-2.0f, -2.0f, 0.0f));
    auto transform = glm::translate(glm::scale(glm::mat4(1.0f), 0.01f * glm::vec3(1, 1, 1)), glm::vec3(-250.0f, -150.0f, 300.0f));
    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj", transform); //
    //auto colorTexture = std::make_shared<ConstantTexture>(glm::vec3(0.6f, 0.4f, 0.9f));
    auto roughness = std::make_shared<ConstantTexture<float>>(0.0f); //

    for (size_t i = 0; i < meshes.size(); i++) {
        auto& mesh = meshes[i];
        auto meshPtr = std::make_shared<TriangleMesh>(std::move(mesh));

        if (i == 0) {
            // Back box
            auto kd = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto t = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.5f));
            auto r = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.5f));
            auto ks = std::make_shared<ConstantTexture<Spectrum>>(Spectrum(0.2f, 0.7f, 0.2f));
            auto translucentMaterialRoughness = std::make_shared<ConstantTexture<float>>(1.0f);
            //auto material = std::make_shared<MatteMaterial>(kd, roughness);
            auto material = std::make_shared<TranslucentMaterial>(kd, ks, translucentMaterialRoughness, r, t, true);
            scene.addSceneObject(std::make_unique<InCoreGeometricSceneObject>(meshPtr, material));
        } else if (i == 1) {
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
}*/
