// clang-format off
#include <pandora/graphics_core/sensor.h>
// clang-format on
#include <pandora/config.h>
#include <pandora/core/stats.h>
#include <pandora/graphics_core/load_from_file.h>
#include <pandora/integrators/naive_direct_lighting_integrator.h>
#include <pandora/integrators/normal_debug_integrator.h>
#include <pandora/integrators/path_integrator.h>
#include <pandora/materials/matte_material.h>
#include <pandora/shapes/triangle.h>
#include <pandora/textures/constant_texture.h>
#include <pandora/traversal/batching_acceleration_structure.h>
#include <pandora/traversal/embree_acceleration_structure.h>
#include <pandora/graphics_core/perspective_camera.h>
#include <pandora/graphics_core/scene.h>
#include <pbrt/pbrt_importer.h>
#include <stream/task_graph.h>
#include <ui/fps_camera_controls.h>
#include <ui/framebuffer_gl.h>
#include <ui/window.h>
#include <pbf/pbf_importer.h>
#include <pandora/graphics_core/load_from_file.h>
#include <stream/cache/dummy_cache.h>
#include <stream/cache/lru_cache.h>
#include <stream/serialize/in_memory_serializer.h>
#include <optick.h>
#include <optick_tbb.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <tbb/blocked_range2d.h>
#include <tbb/parallel_for.h>
#include <xmmintrin.h>

using namespace pandora;
using namespace atlas;
using namespace std::string_literals;

const std::string projectBasePath = "../../"s;

int main(int argc, char** argv)
{
    auto colorLogger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("color_logger");
    spdlog::set_default_logger(colorLogger);
    //spdlog::set_level(spdlog::level::critical);

    // https://embree.github.io/api.html
    // For optimal Embree performance
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

    OPTICK_APP("Atlas");
    Optick::setThisMainThreadOptick();

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
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
    } catch (boost::wrapexcept<boost::program_options::unknown_option> e) {
        std::cout << "Received unknown option " << e.get_option_name() << std::endl;
        exit(1);
    }

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
    tasking::LRUCache::Builder cacheBuilder { std::make_unique<tasking::InMemorySerializer>() };
    //tasking::DummyCache::Builder cacheBuilder;

    const std::filesystem::path sceneFilePath = vm["file"].as<std::string>();
    const unsigned cameraID = 0;
    RenderConfig renderConfig;
    if (sceneFilePath.extension() == ".pbrt")
        renderConfig = pbrt::loadFromPBRTFile(sceneFilePath, cameraID, &cacheBuilder, false);
    else if (sceneFilePath.extension() == ".pbf")
        renderConfig = pbf::loadFromPBFFile(sceneFilePath, cameraID, &cacheBuilder, false);
    else if (sceneFilePath.extension() == ".json")
        renderConfig = loadFromFile(sceneFilePath);
    else {
        spdlog::error("Unknown scene file extension {}", sceneFilePath.extension().string());
        exit(1);
    }
    const glm::ivec2 resolution = renderConfig.resolution;
    auto geometryCache = cacheBuilder.build(500llu * 1024 * 1024 * 1024);

#define BATCHING_ACCEL 1

#ifdef BATCHING_ACCEL
    using AccelBuilder = BatchingAccelerationStructureBuilder;
#else
    using AccelBuilder = EmbreeAccelerationStructureBuilder;
#endif

    if constexpr (std::is_same_v<AccelBuilder, EmbreeAccelerationStructureBuilder>) {
        std::function<void(const std::shared_ptr<SceneNode>&)> makeShapeResident = [&](const std::shared_ptr<SceneNode>& pSceneNode) {
            for (const auto& pSceneObject : pSceneNode->objects) {
                geometryCache.makeResident(pSceneObject->pShape.get());
            }

            for (const auto& [pChild, _] : pSceneNode->children) {
                makeShapeResident(pChild);
            }
        };
        makeShapeResident(renderConfig.pScene->pRoot);
    }

    Window myWindow(resolution.x, resolution.y, "Atlas - Pandora viewer");
    FramebufferGL frameBuffer(resolution.x, resolution.y);
    FpsCameraControls cameraControls(myWindow, *renderConfig.camera);

    spdlog::info("Creating integrator");
    tasking::TaskGraph taskGraph;
    const int spp = vm["spp"].as<int>();
    //NormalDebugIntegrator integrator { &taskGraph };
    //DirectLightingIntegrator integrator { &taskGraph, &geometryCache, 8, spp, LightStrategy::UniformSampleOne };
    PathIntegrator integrator { &taskGraph, &geometryCache, 8, spp, LightStrategy::UniformSampleOne };

    spdlog::info("Preprocessing scene");
    constexpr unsigned primitivesPerBatchingPoint = 5000000;
    if constexpr (std::is_same_v<AccelBuilder, BatchingAccelerationStructureBuilder>) {
        cacheBuilder = tasking::LRUCache::Builder { std::make_unique<tasking::InMemorySerializer>() };
#ifdef BATCHING_ACCEL
        AccelBuilder::preprocessScene(*renderConfig.pScene, geometryCache, cacheBuilder, primitivesPerBatchingPoint);
#endif
        auto newCache = cacheBuilder.build(geometryCache.maxSize());
        geometryCache = std::move(newCache);
    }

    spdlog::info("Building acceleration structure");
#ifdef BATCHING_ACCEL
    AccelBuilder accelBuilder { renderConfig.pScene.get(), &geometryCache, &taskGraph, primitivesPerBatchingPoint, 128llu * 1024 * 1024 * 1024, 0 };
#else
    AccelBuilder accelBuilder { *renderConfig.pScene, &taskGraph };
#endif
    auto accel = accelBuilder.build(integrator.hitTaskHandle(), integrator.missTaskHandle(), integrator.anyHitTaskHandle(), integrator.anyMissTaskHandle());

    bool pressedEscape = false;
    myWindow.registerKeyCallback([&](int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            pressedEscape = true;
    });

    auto& camera = *renderConfig.camera;
    Sensor sensor { resolution };
    auto previousTimestamp = std::chrono::high_resolution_clock::now();
    int samples = 0;
    while (!myWindow.shouldClose() && !pressedEscape) {
        OPTICK_FRAME("MainThread");

        myWindow.updateInput();
        cameraControls.tick();

        if (cameraControls.cameraChanged()) {
            samples = 0;
            sensor.clear(glm::vec3(0.0f));
        }

#if 1
        integrator.render(500 * 1000, camera, sensor, *renderConfig.pScene, accel, samples);
#else
        samples = 0;
        sensor.clear(glm::vec3(0));

        const glm::vec2 fResolution = resolution;
        tbb::blocked_range2d range { 0, resolution.x, 0, resolution.y };
        tbb::parallel_for(range, [&](tbb::blocked_range2d<int> subRange) {
            Optick::tryRegisterThreadWithOptick();
            OPTICK_EVENT("RenderTile");

            for (int y = subRange.cols().begin(); y < subRange.cols().end(); y++) {
                for (int x = subRange.rows().begin(); x < subRange.rows().end(); x++) {
                    Ray cameraRay = renderConfig.camera->generateRay(glm::vec2(x, y) / fResolution);
                    auto siOpt = accel.intersectDebug(cameraRay);
                    if (siOpt) {
                        const float cos = glm::dot(siOpt->normal, -cameraRay.direction);
                        sensor.addPixelContribution(glm::ivec2 { x, y }, cos * siOpt->shading.batchingPointColor);
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

    OPTICK_SHUTDOWN();
    return 0;
}
