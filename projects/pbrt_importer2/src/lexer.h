#pragma once
#include <string_view>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.h

namespace pbrt_importer {

struct Loc {
    int line { -1 };
    int col { -1 };
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
    std::string_view text;
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

}