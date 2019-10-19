#pragma once
#include "lexer.h"
#include <filesystem>
#include <fstream>
#include <string>

// Matching the implementation of Ingo Wald for performance comparisons
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h
struct WaldToken {
    Loc loc {};
    TokenType type { TokenType::NONE };
    std::string text;

    inline bool operator==(const Token& other) const
    {
        return loc.line == other.loc.line && loc.col == other.loc.col && type == other.type;
    }
};

class WaldLexer {
public:
    WaldLexer(std::filesystem::path fileName);

    WaldToken next();

private:
    inline int getChar();
    inline void ungetChar(int c);

private:
    FILE* m_pFile;

    Loc m_location;
    int peekedChar { -1 };
};
