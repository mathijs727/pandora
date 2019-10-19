#include "simd_lexer.h"

#include "lexer.h"
#include <array>
#include <nmmintrin.h>
#include <spdlog/spdlog.h>

// Inspiration taken from pbrt-parser by Ingo Wald:
// https://github.com/ingowald/pbrt-parser/blob/master/pbrtParser/impl/syntactic/Lexer.cpp

inline bool isSpecial(const char c) noexcept
{
    return (c == '[' || c == ']' || c == ',');
}
inline bool isWhiteSpace(const char c) noexcept
{
    return (c == ' ' || c == '\t' || c == '\r' || c == '\n');
}

SIMDLexer::SIMDLexer(std::string_view text)
    : m_text(text)
    , m_stringLengthSSEBounds(text.length() - 16) // Instead of checking [cursor+16 < string.length()] we can check [cursor < string.length()-16] (very slightly faster)
{
    if (text.length() < 16) {
        spdlog::error("File smaller than 16 bytes causing size_t underflow in m_stringLengthSSEBounds");
        throw std::runtime_error("size_t underflow!");
	}
}

Token SIMDLexer::next() noexcept
{
	// Skip white space
    char c;
    while (true) {
        c = getChar();

        // End of file
        if (c == -1)
            return Token {};

        if (c == '\n')
            newLine();

        if (isWhiteSpace(c))
            continue;

        // Comment
        if (c == '#') {
            while (c != '\n') {
                c = getChar();
                if (c < 0)
                    return Token {};
            }
            newLine();
            continue;
        }
        break;
    }

    const Loc tokenStartLoc = m_location;
    const size_t tokenStart = m_cursor;

    switch (c) {
    case '"': { // String
        //moveCursor(1);
        while (true) {
            c = getChar();
            if (c < 0) {
                spdlog::error("Could not find end of string literal (found eof instead)");
                return Token {};
            }
            if (c == '"')
                break;
        }
        return Token { tokenStartLoc, TokenType::STRING, m_text.substr(tokenStart + 1, m_cursor - tokenStart - 2) };
    } break;
    case '[': {
        //moveCursor(1);
        return Token { tokenStartLoc, TokenType::LIST_BEGIN, m_text.substr(tokenStart, 1) };
    } break;
    case ']': {
        //moveCursor(1);
        return Token { tokenStartLoc, TokenType::LIST_END, m_text.substr(tokenStart, 1) };
    } break;
    default: {
        // Literals
        alignas(16) const char literalsMaskChars[16] { '#', '[', ',', ']', ' ', '\t', '\r', '\n', '"', -1, -1, -1, -1, -1, -1, -1 };
        const __m128i literalsMask = _mm_load_si128(reinterpret_cast<const __m128i*>(literalsMaskChars));
        while (true) {
            const __m128i peekedChars = peekCharsSSE();
            const int offset = _mm_cmpistri(literalsMask, peekedChars, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);
            moveCursor(offset);
            if (offset != 16) {
                return Token { tokenStartLoc, TokenType::LITERAL, m_text.substr(tokenStart, m_cursor - tokenStart) };
            }
        }

        // Should never reached
        assert(false);
        return Token {};
    } break;
    }
}
