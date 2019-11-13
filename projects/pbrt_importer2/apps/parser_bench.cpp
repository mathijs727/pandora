#include "pbrt/parser/parser.h"
#include "timer.h"

#include <boost/program_options.hpp>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

boost::program_options::variables_map parseInput(int argc, const char** argv);

int main(int argc, const char** argv)
{
    auto colorLogger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("color_logger");
    spdlog::set_default_logger(colorLogger);

    auto input = parseInput(argc, argv);

    std::filesystem::path filePath = input["file"].as<std::string>();
    std::filesystem::path basePath = filePath;
    basePath.remove_filename();
    Parser parser { basePath };

    {
        Timer timer;
        parser.parse(filePath, 0);
        auto elapsed = timer.elapsed<std::chrono::milliseconds>();

        spdlog::info("Time to parse: {}ms", elapsed.count());
    }

    return 0;
}

boost::program_options::variables_map parseInput(int argc, const char** argv)
{
    namespace po = boost::program_options;
    po::options_description desc("Pandora options");
    // clang-format off
    desc.add_options()
		("file", po::value<std::string>()->required(), "PBRT file to parse")
		("help", "show all arguments");
    // clang-format on

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(1);
    }

    try {
        po::notify(vm);
    } catch (const boost::program_options::required_option& e) {
        std::cout << fmt::format("Missing required argument \"{}\"", e.get_option_name()) << std::endl;
        exit(1);
    }

    return vm;
}
