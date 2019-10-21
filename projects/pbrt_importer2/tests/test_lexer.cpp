#include "pbrt/lexer/lexer.h"
#include "pbrt/lexer/simd_lexer.h"
#include "pbrt/lexer/wald_lexer.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

std::string readFile(std::filesystem::path file);
const std::filesystem::path testFileFolder { "test_files" };

TEST(LexerScalar, Crown)
{
    const std::filesystem::path filePath = testFileFolder / "crown.pbrt";
    const std::string fileContents = readFile(filePath);

    Lexer lexer { fileContents };
    WaldLexer waldLexer { filePath };

    while (true) {
        Token token = lexer.next();
        WaldToken waldToken = waldLexer.next();

		if (token.text != waldToken.text)
            int i = 3;
        ASSERT_EQ(token.text, waldToken.text);
        ASSERT_EQ(token.type, waldToken.type);

        if (token.type == TokenType::NONE)
            break;
    }
}

TEST(LexerScalar, Landscape)
{
    const std::filesystem::path filePath = testFileFolder / "landscape_geometry.pbrt";
    const std::string fileContents = readFile(filePath);

    Lexer lexer { fileContents };
    WaldLexer waldLexer { filePath };

    while (true) {
        Token token = lexer.next();
        WaldToken waldToken = waldLexer.next();

        ASSERT_EQ(token.type, waldToken.type);
        ASSERT_EQ(token.text, waldToken.text);

        if (token.type == TokenType::NONE)
            break;
    }
}

TEST(LexerSIMD, Crown)
{
    const std::filesystem::path filePath = testFileFolder / "crown.pbrt";
    const std::string fileContents = readFile(filePath);

    SIMDLexer lexer { fileContents };
    WaldLexer waldLexer { filePath };

    while (true) {
        Token token = lexer.next();
        WaldToken waldToken = waldLexer.next();

        ASSERT_EQ(token.type, waldToken.type);
        ASSERT_EQ(token.text, waldToken.text);

        if (token.type == TokenType::NONE)
            break;
    }
}

TEST(LexerSIMD, Landscape)
{
    const std::filesystem::path filePath = testFileFolder / "landscape_geometry.pbrt";
    const std::string fileContents = readFile(filePath);

    SIMDLexer lexer { fileContents };
    WaldLexer waldLexer { filePath };

    while (true) {
        Token token = lexer.next();
        WaldToken waldToken = waldLexer.next();

        ASSERT_EQ(token.type, waldToken.type);
        ASSERT_EQ(token.text, waldToken.text);

        if (token.type == TokenType::NONE)
            break;
    }
}

std::string readFile(std::filesystem::path file)
{
    assert(std::filesystem::exists(file) && std::filesystem::is_regular_file(file));

    std::ifstream ifs { file, std::ios::in | std::ios::binary | std::ios::ate };

    auto fileSize = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    std::string out;
    out.resize(fileSize);
    ifs.read(out.data(), fileSize);
    return out;
}
