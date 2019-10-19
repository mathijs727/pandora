#pragma once
#include "lexer.h"
#include <nmmintrin.h>

class SIMDLexer {
public:
    SIMDLexer(std::string_view text);

    Token next() noexcept;

private:
    inline __m128i peekCharsSSE() const noexcept;
    inline char getChar() noexcept;

	inline void moveCursor(int offset) noexcept;

private:
    std::string_view m_text;
    size_t m_stringLengthSSEBounds;

    size_t m_cursor { 0 };
};

inline __m128i SIMDLexer::peekCharsSSE() const noexcept
{
    // Seems to work fine without bounds check on Windows (it's measureably faster) but seems like undefined behavior?
    //if constexpr (true) {
    //if (m_cursor + 16 < m_text.length()) {
    if (m_cursor < m_stringLengthSSEBounds) {
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

    return m_text[m_cursor++];
}

inline void SIMDLexer::moveCursor(int offset) noexcept
{
    m_cursor += static_cast<size_t>(offset);
}
