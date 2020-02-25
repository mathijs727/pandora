// clang-format off
#include "pandora/graphics_core/sensor.h"
// clang-format on
#include "output.h"
#include "pandora/config.h"
#include "pandora/core/stats.h"
#include "pandora/graphics_core/load_from_file.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/normal_debug_integrator.h"
#include "pandora/integrators/path_integrator.h"
//#include "pandora/integrators/svo_depth_test_integrator.h"
//#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/materials/matte_material.h"
#include "pandora/shapes/triangle.h"
#include "pandora/textures/constant_texture.h"
#include "stream/task_graph.h"
#include <optick.h>
#include <optick_tbb.h>
#include <pandora/traversal/offline_batching_acceleration_structure.h>
#include <pandora/traversal/batching_acceleration_structure.h>
#include <pandora/traversal/embree_acceleration_structure.h>
#include <pbrt/pbrt_importer.h>
#include <pbf/pbf_importer.h>
#include <stream/cache/lru_cache.h>
#include <stream/cache/lru_cache_ts.h>
#include <stream/serialize/file_serializer.h>
#include <stream/serialize/in_memory_serializer.h>
#include <pandora/graphics_core/perspective_camera.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <string>
#include <tbb/tbb.h>
#include <xmmintrin.h>
#include <unordered_set>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

using namespace pandora;
using namespace torque;

#define OUTPUT_PROFILE_DATA 1

int main(int argc, char** argv)
{
#if OUTPUT_PROFILE_DATA
    OPTICK_APP("Torque");
    Optick::setThisMainThreadOptick();
#endif


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
		("subdiv", po::value<unsigned>()->default_value(0), "Number of times to subdivide each triangle in the scene")
		("cameraid", po::value<unsigned>()->default_value(0), "Camera ID (index of occurence in pbrt/pbf file)")
		("out", po::value<std::string>()->default_value("output"), "output name (without file extension!)")
		("integrator", po::value<std::string>()->default_value("direct"), "integrator (normal, direct or path)")
		("spp", po::value<int>()->default_value(1), "samples per pixel")
		("concurrency", po::value<unsigned>()->default_value(500*1000), "Number of paths traced concurrently")
		("schedulers", po::value<unsigned>()->default_value(2), "Number of scheduler tasks spawned concurrently")
		("geomcache", po::value<size_t>()->default_value(100 * 1000), "Geometry cache size (MB)")
		("bvhcache", po::value<size_t>()->default_value(100 * 1000), "Bot level BVH cache size (MB)")
		("primgroup", po::value<unsigned>()->default_value(1000 * 1000), "Number of primitives per batching point")
		("svdagres", po::value<unsigned>()->default_value(128), "Resolution of the voxel grid used to create the SVDAG")
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

    const unsigned subdiv = vm["subdiv"].as<unsigned>();
    const unsigned cameraID = vm["cameraid"].as<unsigned>();
    int spp = vm["spp"].as<int>();
    const unsigned concurrency = vm["concurrency"].as<unsigned>();
    const unsigned schedulers = vm["schedulers"].as<unsigned>();
    const size_t geomCacheSizeMB = vm["geomcache"].as<size_t>();
    const size_t bvhCacheSizeMB = vm["bvhcache"].as<size_t>();
    const size_t geomCacheSize = geomCacheSizeMB * 1000000;
    const size_t bvhCacheSize = bvhCacheSizeMB * 1000000;
    const unsigned primitivesPerBatchingPoint = vm["primgroup"].as<unsigned>();
    const unsigned svdagRes = vm["svdagres"].as<unsigned>();

    std::cout << "Rendering with the following settings:\n";
    std::cout << "  file:           " << vm["file"].as<std::string>() << "\n";
    std::cout << "  subdiv:         " << subdiv << "\n";
    std::cout << "  camera ID:      " << cameraID << "\n";
    std::cout << "  out:            " << vm["out"].as<std::string>() << "\n";
    std::cout << "  integrator:     " << vm["integrator"].as<std::string>() << "\n";
    std::cout << "  spp:            " << spp << std::endl;
    std::cout << "  concurrency:    " << concurrency << "\n";
    std::cout << "  schedulers:     " << schedulers << "\n";
    std::cout << "  geom cache:     " << geomCacheSizeMB << "MB\n";
    std::cout << "  bot bvh cache:  " << bvhCacheSizeMB << "MB\n";
    std::cout << "  batching point: " << primitivesPerBatchingPoint << " primitives\n";
    std::cout << "  svdag res:      " << svdagRes << "\n";
    std::cout << std::flush;

    g_stats.config.sceneFile = vm["file"].as<std::string>();
    g_stats.config.subdiv = subdiv;
    g_stats.config.cameraID = cameraID;

    g_stats.config.integrator = vm["integrator"].as<std::string>();
    g_stats.config.spp = spp;
    g_stats.config.concurrency = concurrency;
    g_stats.config.schedulers = schedulers;

    g_stats.config.geomCacheSize = geomCacheSize;
    g_stats.config.bvhCacheSize = bvhCacheSize;
    g_stats.config.primGroupSize = primitivesPerBatchingPoint;
    g_stats.config.svdagRes = svdagRes;

    spdlog::info("Loading scene");
    // WARNING: This cache is not used during rendering when using the batched acceleration structure.
    //          A new cache is instantiated when splitting the scene into smaller objects. Scroll down...
    auto pSerializer = std::make_unique<tasking::InMemorySerializer>();
    //auto pSerializer = std::make_unique<tasking::SplitFileSerializer>(
    //    "pandora_pre_geom", 512 * 1024 * 1024, mio_cache_control::cache_mode::sequential);
    tasking::LRUCacheTS::Builder cacheBuilder { std::move(pSerializer) };

    RenderConfig renderConfig;
    {
        OPTICK_EVENT("loadFromFile");
        auto stopWatch = g_stats.timings.loadFromFileTime.getScopedStopwatch();
        const std::filesystem::path sceneFilePath = vm["file"].as<std::string>();
        if (sceneFilePath.extension() == ".pbrt")
            renderConfig = pbrt::loadFromPBRTFile(sceneFilePath, cameraID, &cacheBuilder, subdiv, false);
        else if (sceneFilePath.extension() == ".pbf")
            renderConfig = pbf::loadFromPBFFile(sceneFilePath, cameraID, &cacheBuilder, subdiv, false);
        else if (sceneFilePath.extension() == ".json")
            renderConfig = loadFromFile(sceneFilePath);
        else {
            spdlog::error("Unknown scene file extension {}", sceneFilePath.extension().string());
            exit(1);
        }
    }
    const glm::ivec2 resolution = renderConfig.resolution;
    tasking::LRUCacheTS geometryCache = cacheBuilder.build(geomCacheSize);

    // Store geometry loaded data before we start splitting the large shapes as part of preprocess.
    g_stats.asyncTriggerSnapshot();

    tasking::TaskGraph taskGraph { schedulers };

    //using AccelBuilder = EmbreeAccelerationStructureBuilder;
    using AccelBuilder = BatchingAccelerationStructureBuilder;
    //using AccelBuilder = OfflineBatchingAccelerationStructureBuilder;
    if constexpr (std::is_same_v<AccelBuilder, BatchingAccelerationStructureBuilder> || std::is_same_v<AccelBuilder, OfflineBatchingAccelerationStructureBuilder>) {
        spdlog::info("Preprocessing scene");
        //auto pSerializer = std::make_unique<tasking::SplitFileSerializer>(
        //    "pandora_render_geom", 512 * 1024 * 1024, mio_cache_control::cache_mode::no_buffering);
        auto pSerializer = std::make_unique<tasking::InMemorySerializer>();

        cacheBuilder = tasking::LRUCacheTS::Builder { std::move(pSerializer) };
        AccelBuilder::preprocessScene(*renderConfig.pScene, geometryCache, cacheBuilder, primitivesPerBatchingPoint);
        auto newCache = cacheBuilder.build(geometryCache.maxSize());
        geometryCache = std::move(newCache);
    }

    // Reset stats so that geometry loaded / evicted only contains the data from during the render, not the loading and preprocess.
    g_stats.memory.geometryEvicted = 0;
    g_stats.memory.geometryLoaded = 0;

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

    spdlog::info("Building acceleration structure");
    //AccelBuilder accelBuilder { *renderConfig.pScene, &taskGraph };
    AccelBuilder accelBuilder { renderConfig.pScene.get(), &geometryCache, &taskGraph, primitivesPerBatchingPoint, bvhCacheSize, svdagRes };
    Sensor sensor { renderConfig.resolution };

    try {
        auto integratorType = vm["integrator"].as<std::string>();

        auto render = [&](auto& integrator) {
            spdlog::info("Building acceleration structure");
            auto accel = accelBuilder.build(integrator.hitTaskHandle(), integrator.missTaskHandle(), integrator.anyHitTaskHandle(), integrator.anyMissTaskHandle());

            // Offline BVH generates BVHs and evicts them immediately after construction. Clear stats so that final stats
            // show only BVH traffic during rendering.
            g_stats.asyncTriggerSnapshot();
            g_stats.memory.botLevelEvicted = 0;
            g_stats.memory.botLevelLoaded = 0;

            spdlog::info("Starting render");
            auto stopWatch = g_stats.timings.totalRenderTime.getScopedStopwatch();
            integrator.render(concurrency, *renderConfig.camera, sensor, *renderConfig.pScene, accel);
        };

        if (integratorType == "direct") {
            DirectLightingIntegrator integrator(&taskGraph, &geometryCache, 8, spp, LightStrategy::UniformSampleOne);
            render(integrator);
        } else if (integratorType == "path") {
            PathIntegrator integrator { &taskGraph, &geometryCache, 8, spp, LightStrategy::UniformSampleOne };
            render(integrator);

        } else if (integratorType == "normal") {
            if (spp != 1)
                spdlog::warn("Normal visualization does not support multi-sampling, setting spp to 1!");
            spp = g_stats.config.spp = 1;

            NormalDebugIntegrator integrator { &taskGraph };
            render(integrator);
        }
    } catch (const std::exception& e) {
        std::cout << "Render error: " << e.what() << std::endl;
    }

    spdlog::info("Writing output to {}.jpg/exr", vm["out"].as<std::string>());
    writeOutputToFile(sensor, spp, vm["out"].as<std::string>() + ".jpg", true);
    writeOutputToFile(sensor, spp, vm["out"].as<std::string>() + ".exr", false);

    spdlog::info("Shutting down...");
    return 0;
}
