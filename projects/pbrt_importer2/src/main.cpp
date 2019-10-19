#include "lexer.h"
#include "simd_lexer.h"
#include "timer.h"

#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#else
#include <spdlog/sinks/stdout_color_sinks.h>
#endif
#include <cassert>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <string>

boost::program_options::variables_map parseInput(int argc, const char** argv);
std::string readFile(std::filesystem::path file);

template <typename T>
void benchLexer(std::string_view fileContents);
template <typename T>
void printLexer(std::string_view fileContents);

void compareLexers(std::string_view fileContents);

int main(int argc, const char** argv)
{
#ifdef _WIN32
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);
#else
    auto colorLogger = spdlog::create<spdlog::sinks::stdout_color_sink_mt>("color_logger");
    spdlog::set_default_logger(colorLogger);
#endif

    auto input = parseInput(argc, argv);

    std::filesystem::path filePath = input["file"].as<std::string>();
    std::string fileContents = readFile(filePath);

#if 1
    benchLexer<SIMDLexer>(fileContents);
    benchLexer<Lexer>(fileContents);
#else
    //printLexer<SIMDLexer>(fileContents);
    compareLexers(fileContents);
#endif

    return 0;
}

template <typename T>
void benchLexer(std::string_view fileContents)
{
    T lexer { fileContents };

    Timer timer {};
    size_t numTokens { 0 };
    while (true) {
        Token token = lexer.next();
        if (token.type == TokenType::NONE)
            break;

        numTokens++;
    }
    auto elapsed = timer.elapsed<std::chrono::milliseconds>();
    spdlog::info("{} tokenized {:.3f}MB in {}ms", typeid(T).name(), fileContents.size() / (1000 * 1000.0f), elapsed.count());
    spdlog::info("Number of tokens: {}", numTokens);
}

template <typename T>
void printLexer(std::string_view fileContents)
{
    T lexer { fileContents };

    while (true) {
        Token token = lexer.next();
        if (token.type == TokenType::NONE)
            break;

        switch (token.type) {
        case TokenType::STRING: {
            spdlog::info("STRING:  \"{}\"", token.text);
        } break;
        case TokenType::LITERAL: {
            spdlog::info("LITERAL: \"{}\"", token.text);
        } break;
        case TokenType::LIST_BEGIN: {
            spdlog::info("LIST_BEGIN: {}", token.text);
        } break;
        case TokenType::LIST_END: {
            spdlog::info("LIST_END: {}", token.text);
        } break;
        };
    }
}
void compareLexers(std::string_view fileContents)
{
    Lexer lexer { fileContents };
    SIMDLexer simdLexer { fileContents };

    while (true) {
        Token token = lexer.next();
        Token simdToken = simdLexer.next();
        spdlog::info("\"{}\" vs \"{}\"", token.text, simdToken.text);
        assert(token == simdToken);

		if (token.type == TokenType::NONE)
            break;
    }
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

std::string readFile(std::filesystem::path file)
{
    assert(std::filesystem::exists(file) && std::filesystem::is_regular_file(file));

    std::ifstream t { file };
    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    return str;
}
