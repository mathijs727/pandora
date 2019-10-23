#include "pbrt/lexer/lexer.h"
#include <array>
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

Lexer::Lexer(std::string_view text)
    : m_text(text)
{
}

Token Lexer::next() noexcept
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
        return Token { TokenType::STRING, m_text.substr(tokenStart + 1, m_cursor - tokenStart - 2) };
    }

    // Special characters
    if (c == '[') {
        return Token { TokenType::LIST_BEGIN, m_text.substr(tokenStart, 1) };
    }
    if (c == ']') {
        return Token { TokenType::LIST_END, m_text.substr(tokenStart, 1) };
    }

    // Literals
    while (true) {
        const char nextC = peekNextChar();
        if (nextC == '#' || isSpecial(nextC) || isWhiteSpace(nextC) || nextC == '"' || nextC == -1) {
            return Token { TokenType::LITERAL, m_text.substr(tokenStart, m_cursor - tokenStart) };
        }

        c = getChar();
        //if (c < 0)
        //    return Token { tokenStartLoc, TokenType::LITERAL, m_text.substr(tokenStart, m_cursor - tokenStart - 1) };
    }

    // Should never reached
    assert(false);
    return Token {};
}

inline char Lexer::getChar() noexcept
{
    if (m_cursor >= m_text.length())
        return -1;

    return m_text[m_cursor++];
}

inline char Lexer::peekNextChar() noexcept
{
    if (m_cursor >= m_text.length())
        return -1;

    return m_text[m_cursor];
}
