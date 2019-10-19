#pragma once
#include "lexer.h"
#include <nmmintrin.h>

class SIMDLexer {
public:
    SIMDLexer(std::string_view text);

    Token next() noexcept;

private:
    inline __m128i peekCharsSSE() noexcept;
    inline char getChar() noexcept;
    inline char peekNextChar() noexcept;

private:
    std::string_view m_text;
    size_t m_stringLengthSSEBounds;

    size_t m_cursor { 0 };
    Loc m_location;
};
