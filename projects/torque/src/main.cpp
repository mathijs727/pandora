#include "pandora/core/load_from_file.h"
#include "pandora/integrators/direct_lighting_integrator.h"
#include "pandora/integrators/naive_direct_lighting_integrator.h"
#include "pandora/integrators/normal_debug_integrator.h"
#include "pandora/integrators/path_integrator.h"
#include "pandora/integrators/svo_depth_test_integrator.h"
#include "pandora/integrators/svo_test_integrator.h"
#include "pandora/config.h"
#include "output.h"
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
    desc.add_options()
        ("file", po::value<std::string>()->required(), "Pandora scene description JSON")
        ("out", po::value<std::string>()->default_value("output"s), "output name (without file extension!)")
        ("integrator", po::value<std::string>()->default_value("direct"), "integrator (normal, direct or path)")
        ("spp", po::value<int>()->default_value(1), "samples per pixel")
        ("help", "show all arguments");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    try {
        po::notify(vm);
    }
    catch (const boost::program_options::required_option& e) {
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

    auto renderConfig = pandora::OUT_OF_CORE_ACCELERATION_STRUCTURE ? loadFromFileOOC(vm["file"].as<std::string>(), false) : loadFromFile(vm["file"].as<std::string>(), false);

    /*// Skydome
    auto colorTexture = std::make_shared<ImageTexture<Spectrum>>(projectBasePath + "assets/skydome/DF360_005_Ref.hdr");
    auto transform = glm::rotate(glm::mat4(1.0f), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    scene.addInfiniteLight(std::make_shared<EnvironmentLight>(transform, Spectrum(0.5f), 1, colorTexture));*/

    if constexpr (pandora::OUT_OF_CORE_ACCELERATION_STRUCTURE) {
        try {
            renderConfig.scene.splitLargeOOCSceneObjects(OUT_OF_CORE_BATCHING_PRIMS_PER_LEAF / 4);
        } catch (std::error_code e) {
            std::cout << "Error splitting ooc scene objects: " << e << std::endl;
            std::cout << "Message: " << e.message() << std::endl;
        }
    }

    std::cout << "Start render" << std::endl;
    int spp = vm["spp"].as<int>();
    try {
        auto integratorType = vm["integrator"].as<std::string>();
        if (integratorType == "direct") {
            DirectLightingIntegrator integrator(8, renderConfig.scene, renderConfig.camera->getSensor(), spp, PARALLEL_SAMPLES, LightStrategy::UniformSampleOne);
            integrator.render(*renderConfig.camera);
        } else if (integratorType == "path") {
            PathIntegrator integrator(10, renderConfig.scene, renderConfig.camera->getSensor(), spp, PARALLEL_SAMPLES);
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
    }

    //NaiveDirectLightingIntegrator integrator(8, scene, camera.getSensor(), spp);
    //SVOTestIntegrator integrator(scene, camera.getSensor(), spp);
    //SVODepthTestIntegrator integrator(scene, camera.getSensor(), spp);

    std::cout << "Writing output: " << vm["out"].as<std::string>() << ".jpg/exr" << std::endl;
    writeOutputToFile(renderConfig.camera->getSensor(), spp, vm["out"].as<std::string>() + ".jpg", true);
    //writeOutputToFile(renderConfig.camera->getSensor(), spp, vm["out"].as<std::string>() + ".exr", false);

    std::cout << "Done" << std::endl;
    return 0;
}
