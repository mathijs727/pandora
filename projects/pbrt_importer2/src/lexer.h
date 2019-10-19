#pragma once
#include <string_view>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h

struct Loc {
    int line { 0 };
    int col { 0 };
};

enum class TokenType {
    STRING,
    LITERAL,
    LIST_BEGIN,
    LIST_END,
    NONE
};
struct Token {
    Loc loc {};
    TokenType type { TokenType::NONE };
    std::string_view text {};

    inline bool operator==(const Token& other) const
    {
        return loc.line == other.loc.line && loc.col == other.loc.col && type == other.type;
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
    Loc m_location;
};
