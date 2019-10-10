#include "output.h"
#include "pandora/config.h"
#include "pandora/core/stats.h"
#include "pandora/geometry/triangle.h"
#include "pandora/graphics_core/load_from_file.h"
#include "pandora/integrators/direct_lighting_integrator.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/normal_debug_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/integrators/svo_depth_test_integrator.h"
#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/materials/matte_material.h"
#include "pandora/textures/constant_texture.h"
#include "stream/task_graph.h"
#include <xmmintrin.h>

#include <boost/program_options.hpp>
#include <string>
#include <tbb/tbb.h>
#include <xmmintrin.h>

using namespace pandora;
using namespace torque;
using namespace std::string_literals;

const std::string projectBasePath = "../../"s;

int main(int argc, char** argv)
{
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

    pandora::RenderConfig renderConfig;
    renderConfig.resolution = glm::ivec2(1280, 720);
    {
        glm::mat4 transform { 1.0f };
        transform = glm::translate(transform, glm::vec3(0, 0, -5));
        renderConfig.camera = std::make_unique<PerspectiveCamera>(renderConfig.resolution, 45.0f, transform);
    }

    auto meshes = TriangleMesh::loadFromFile("C:/Users/mathi/Documents/GitHub/pandora/assets/3dmodels/monkey.obj");
    for (TriangleMesh& mesh : meshes) {
        auto pMesh = std::make_shared<TriangleMesh>(std::move(mesh));
        auto pSceneObject = std::make_shared<SceneObject>(SceneObject { pMesh, nullptr, nullptr });
        renderConfig.scene.m_root.objects.push_back(std::move(pSceneObject));
    }

    std::cout << "Start render" << std::endl;
    int spp = vm["spp"].as<int>();
    /*try {
        auto integratorType = vm["integrator"].as<std::string>();
        if (integratorType == "direct") {
            DirectLightingIntegrator integrator(8, renderConfig.scene, renderConfig.camera->getSensor(), spp, LightStrategy::UniformSampleOne);
            integrator.render(*renderConfig.camera);
        } else if (integratorType == "path") {
            PathIntegrator integrator(10, renderConfig.scene, renderConfig.camera->getSensor(), spp);
            std::cout << "Start rendering..." << std::endl;
            integrator.render(*renderConfig.camera);
        } else if (integratorType == "normal") {
            std::cout << "WARNING: normal visualization does not support multi-sampling, setting spp to 1!" << std::endl;
            spp = 1;
            g_stats.config.spp = 1;

            NormalDebugIntegrator integrator(renderConfig.scene, renderConfig.camera->getSensor());
            integrator.render(*renderConfig.camera);
        }
    } catch (const std::exception& e) {
        std::cout << "Render error: " << e.what() << std::endl;
    }*/

    //NaiveDirectLightingIntegrator integrator(8, scene, camera.getSensor(), spp);
    //SVOTestIntegrator integrator(scene, camera.getSensor(), spp);
    //SVODepthTestIntegrator integrator(scene, camera.getSensor(), spp);
    tasking::TaskGraph taskGraph;
    NormalDebugIntegrator integrator { &taskGraph };

    EmbreeAccelerationStructureBuilder accelBuilder { renderConfig.scene, &taskGraph };
    auto accel = accelBuilder.build(integrator.hitTaskHandle(), integrator.missTaskHandle(), integrator.anyHitTaskHandle(), integrator.anyMissTaskHandle());

    {
        auto renderTimeStopWatch = g_stats.timings.totalRenderTime.getScopedStopwatch();

#if 1
        integrator.render(*renderConfig.camera, renderConfig.camera->getSensor(), accel);
#else
        auto& sensor = renderConfig.camera->getSensor();
        for (int y = 0; y < renderConfig.resolution.y; y++) {
            for (int x = 0; x < renderConfig.resolution.x; x++) {
                CameraSample cameraSample { glm::vec2 { x, y } };
                Ray cameraRay = renderConfig.camera->generateRay(cameraSample);
                auto hitOpt = accel.intersectFast(cameraRay);
                if (hitOpt) {
                    sensor.addPixelContribution(glm::ivec2 { x, y }, hitOpt->geometricNormal);
                }
            }
        }
#endif
    }

    std::cout << "Writing output: " << vm["out"].as<std::string>() << ".jpg/exr" << std::endl;
    writeOutputToFile(renderConfig.camera->getSensor(), spp, vm["out"].as<std::string>() + ".jpg", true);
    writeOutputToFile(renderConfig.camera->getSensor(), spp, vm["out"].as<std::string>() + ".exr", false);

    g_stats.asyncTriggerSnapshot();

    std::cout << "Done" << std::endl;
    return 0;
}

//auto renderConfig = pandora::OUT_OF_CORE_ACCELERATION_STRUCTURE ? loadFromFileOOC(vm["file"].as<std::string>(), false) : loadFromFile(vm["file"].as<std::string>(), false);

/*{
        // Create skylight plane
        Bounds sceneBounds;
        {
            for (const auto& object : renderConfig.scene.getInCoreSceneObjects()) {
                sceneBounds.extend(object->worldBounds());
            }
        }

        auto positions = std::make_unique<glm::vec3[]>(4);
        positions[0] = glm::vec3(sceneBounds.min.x, sceneBounds.min.y, sceneBounds.max.z + sceneBounds.extent().z / 4);
        positions[1] = glm::vec3(sceneBounds.max.x, sceneBounds.min.y, sceneBounds.max.z + sceneBounds.extent().z / 4);
        positions[2] = glm::vec3(sceneBounds.min.x, sceneBounds.max.y, sceneBounds.max.z + sceneBounds.extent().z / 4);
        positions[3] = glm::vec3(sceneBounds.max.x, sceneBounds.max.y, sceneBounds.max.z + sceneBounds.extent().z / 4);

        auto normals = std::make_unique<glm::vec3[]>(4);
        normals[0] = glm::vec3(0, 0, -1);
        normals[1] = glm::vec3(0, 0, -1);
        normals[2] = glm::vec3(0, 0, -1);
        normals[3] = glm::vec3(0, 0, -1);

        auto triangles = std::make_unique<glm::ivec3[]>(2);
        triangles[0] = glm::ivec3(0, 1, 2);
        triangles[1] = glm::ivec3(1, 2, 3);

        auto material = std::make_shared<MatteMaterial>(
            std::make_shared<ConstantTexture<glm::vec3>>(glm::vec3(0.0f)),
            std::make_shared<ConstantTexture<float>>(0.0f));
        auto mesh = std::make_shared<TriangleMesh>(2, 4, std::move(triangles), std::move(positions), std::move(normals), nullptr, nullptr);

        auto lightSceneObject = std::make_unique<InCoreGeometricSceneObject>(mesh, material, Spectrum(1));
        renderConfig.scene.addSceneObject(std::move(lightSceneObject));
    }*/

/*// Skydome
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    auto transform = glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, Spectrum(0.5f), 1, colorTexture));*/

/*if constexpr (pandora::OUT_OF_CORE_ACCELERATION_STRUCTURE) {
        try {
            renderConfig.scene.splitLargeOOCSceneObjects(OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF / 2);
        } catch (std::error_code e) {
            std::cout << "Error splitting ooc scene objects: " << e << std::endl;
            std::cout << "Message: " << e.message() << std::endl;
        }
    }  else {
        try {
            renderConfig.scene.splitLargeInCoreSceneObjects(OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF / 2);
        }
        catch (std::error_code e) {
            std::cout << "Error splitting in-core scene objects: " << e << std::endl;
            std::cout << "Message: " << e.message() << std::endl;
        }
    }*/