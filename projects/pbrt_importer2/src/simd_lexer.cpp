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
    , m_stringLengthSSEBounds(text.length() - 16)
{
}

Token SIMDLexer::next() noexcept
{
    char c;
    while (true) {
        c = getChar();

        // End of file
        if (c == -1)
            return Token {};

        if (isWhiteSpace(c))
            continue;

        // Comment
        if (c == '#') {
            while (c != '\n') {
                c = getChar();
                if (c < 0)
                    return Token {};
            }
            continue;
        }
        break;
    }
    /*alignas(16) const char ignoreMaskChars[16] { ' ', '\t', '\r', '\n', '#', -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    const __m128i ignoreMask = _mm_load_si128(reinterpret_cast<const __m128i*>(ignoreMaskChars));
    while (true) {
        const __m128i peekedChars = peekCharsSSE();
        const int offset = _mm_cmpistri(ignoreMask, peekedChars, _SIDD_UBYTE_OPS | _SIDD_CMP_RANGES);
        m_location.col += offset;
        if (offset != 16) {
            m_cursor += offset + 1;
            return Token { tokenStartLoc, TokenType::LITERAL, m_text.substr(tokenStart, m_cursor - tokenStart - 1) };
        } else {
            m_cursor += offset;
        }
    }*/

    const Loc tokenStartLoc = m_location;
    const size_t tokenStart = m_cursor - 1;

    // String
    if (c == '"') {
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
    }

    // Special characters
    if (c == '[') {
        return Token { tokenStartLoc, TokenType::LIST_BEGIN, m_text.substr(tokenStart, 1) };
    }
    if (c == ']') {
        return Token { tokenStartLoc, TokenType::LIST_END, m_text.substr(tokenStart, 1) };
    }

    // Literals
    alignas(16) const char literalsMaskChars[16] { '#', '[', ',', ']', ' ', '\t', '\r', '\n', '"', -1, -1, -1, -1, -1, -1, -1 };
    const __m128i literalsMask = _mm_load_si128(reinterpret_cast<const __m128i*>(literalsMaskChars));
    while (true) {
        const __m128i peekedChars = peekCharsSSE();
        const int offset = _mm_cmpistri(literalsMask, peekedChars, _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_ANY);
        m_location.col += offset;
        m_cursor += offset;
        if (offset != 16) {
            return Token { tokenStartLoc, TokenType::LITERAL, m_text.substr(tokenStart, m_cursor - tokenStart) };
        }
    }

    // Should never reached
    assert(false);
    return Token {};
}

inline __m128i SIMDLexer::peekCharsSSE() noexcept
{
    // Seems to work fine without bounds check on Windows (it's measureably faster) but seems like undefined behavior?
    //if (m_cursor < m_stringLengthSSEBounds) {
    if constexpr (true) {
        return _mm_loadu_si128(reinterpret_cast<const __m128i*>(m_text.data() + m_cursor));
    } else {
        alignas(16) char chars[16];
        int i;
        for (i = 0; i < m_text.length() - m_cursor; i++)
            chars[i] = m_text[m_cursor + i];

        for (; i < 16; i++)
            chars[i] = -1;

        return _mm_load_si128(reinterpret_cast<const __m128i*>(chars));
    }
}

inline char SIMDLexer::getChar() noexcept
{
    if (m_cursor >= m_text.length())
        return -1;

    const char c = m_text[m_cursor++];
    if (c == '\n') {
        m_location.line++;
        m_location.col = 0;
    } else {
        m_location.col++;
    }
    return c;
}

inline char SIMDLexer::peekNextChar() noexcept
{
    if (m_cursor >= m_text.length())
        return -1;

    return m_text[m_cursor];
}
