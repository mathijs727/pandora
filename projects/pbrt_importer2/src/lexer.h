#pragma once
#include <string_view>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h

enum class TokenType {
    STRING,
    LITERAL,
    LIST_BEGIN,
    LIST_END,
    NONE
};
struct Token {
    TokenType type { TokenType::NONE };
    std::string_view text {};

    inline bool operator==(const Token& other) const
    {
        return type == other.type && text == other.text;
    }
};

class Lexer {
public:
    Lexer(std::string_view text);

    Token next() noexcept;

private:
    inline char getChar() noexcept;
    inline char peekNextChar() noexcept;
    //inline void ungetChar(char c);

private:
    std::string_view m_text;

    size_t m_cursor { 0 };
};
