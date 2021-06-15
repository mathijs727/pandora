#pragma once
#include "lexer.h"
#include <filesystem>
#include <fstream>
#include <string>

// Closely matching the implementation of Ingo Wald for performance comparisons
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h
struct Loc {
    int line { 0 };
    int col { 0 };
};

struct WaldToken {
    Loc loc {};
    TokenType type { TokenType::NONE };
    std::string text;
};

class WaldLexer {
public:
    WaldLexer(std::filesystem::path fileName);
    ~WaldLexer();

    WaldToken next();

private:
    inline int getChar();
    inline void ungetChar(int c);

private:
    FILE* m_pFile;

    Loc m_location;
    int peekedChar { -1 };
};
