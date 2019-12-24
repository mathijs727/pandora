#pragma once
#include "token.h"
#include <string_view>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h

class Lexer {
public:
    Lexer() = default;
    Lexer(std::string_view text);

    Token next() noexcept;

private:
    inline char getChar() noexcept;
    inline char peekNextChar() noexcept;
    //inline void ungetChar(char c);

private:
    std::string_view m_text {};

    size_t m_stringLength { 0 };
    size_t m_cursor { 0 };
};
