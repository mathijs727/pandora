#pragma once
#include "lexer.h"
#include "token.h"
#include <nmmintrin.h>

class SIMDLexer {
public:
    SIMDLexer() = default;
    SIMDLexer(std::string_view text);

    Token next() noexcept;

private:
    inline __m128i peekCharsSSE() const noexcept;
    inline char peekChar() const noexcept;
    inline char getChar() noexcept;

    inline void moveCursor(int64_t offset) noexcept;

private:
    std::string_view m_text;


	int64_t m_stringLength;
    int64_t m_stringLengthMinus16;
    int64_t m_cursor { 0 };
};

inline __m128i SIMDLexer::peekCharsSSE() const noexcept
{
    if (m_cursor < m_stringLengthMinus16) {
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

inline char SIMDLexer::peekChar() const noexcept
{
    if (m_cursor >= m_stringLength)
        return -1;

    return m_text[m_cursor];
}

inline char SIMDLexer::getChar() noexcept
{
    if (m_cursor >= m_stringLength)
        return -1;

    return m_text[m_cursor++];
}

inline void SIMDLexer::moveCursor(int64_t offset) noexcept
{
    m_cursor += offset;
}
