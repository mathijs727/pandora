#include "pbf/lexer.h"
#include "pbf/parser.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <spdlog/spdlog.h>

static boost::program_options::variables_map parseInput(int argc, const char** argv);

int main(int argc, const char** ppArgv)
{
    auto options = parseInput(argc, ppArgv);
    spdlog::info("Hello world!");

    std::filesystem::path file = options["file"].as<std::string>();
    assert(std::filesystem::exists(file));

    pbf::Lexer lexer { file };
    pbf::Parser parser { &lexer };

	parser.parse();

    return 0;
}

static boost::program_options::variables_map parseInput(int argc, const char** argv)
{
    namespace po = boost::program_options;
    po::options_description desc("PBF Lexer Bench options");
    // clang-format off
    desc.add_options()
		("file", po::value<std::string>()->required(), "PBF file to parse")
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