#include "pbrt/lexer/wald_lexer.h"
#include <array>
#include <cstdio>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

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

WaldLexer::WaldLexer(std::filesystem::path file)
    : m_pFile(fopen(file.string().c_str(), "r"))
{
}

WaldToken WaldLexer::next()
{
    int c;

    // Skip all white space and comments
    while (true) {
        c = getChar();

        // End of file
        if (c < 0) {
            if (m_pFile)
                fclose(m_pFile);
            return WaldToken {};
        }

        if (isWhiteSpace(c))
            continue;

        // Comment
        if (c == '#') {
            while (c != '\n') {
                c = getChar();
                if (c < 0)
                    return WaldToken {};
            }
            continue;
        }
        break;
    }

    std::string s;
    s.reserve(100);
    std::stringstream ss { s };

    const Loc tokenStartLoc = m_location;

    // String
    if (c == '"') {
        while (true) {
            c = getChar();
            if (c < 0) {
                spdlog::error("Could not find end of string literal (found eof instead)");
                return WaldToken {};
            }
            if (c == '"')
                break;
            ss << (char)c;
        }
        return WaldToken { tokenStartLoc, TokenType::STRING, ss.str() };
    }

    // Special characters
    if (c == '[') {
        ss << (char)c;
        return WaldToken { tokenStartLoc, TokenType::LIST_BEGIN, ss.str() };
    }
    if (c == ']') {
        ss << (char)c;
        return WaldToken { tokenStartLoc, TokenType::LIST_END, ss.str() };
    }

    // Literals
    ss << (char)c;
    while (true) {
        const char c = getChar();
        if (c < 0)
            return WaldToken { tokenStartLoc, TokenType::LITERAL, ss.str() };

        if (c == '#' || isSpecial(c) || isWhiteSpace(c) || c == '"') {
            ungetChar(c);
            return WaldToken { tokenStartLoc, TokenType::LITERAL, ss.str() };
        }

        ss << (char)c;
    }

    // Should never reached
    assert(false);
    return WaldToken {};
}

inline int WaldLexer::getChar()
{
    if (peekedChar >= 0) {
        int c = peekedChar;
        peekedChar = -1;
        return c;
    }

    if (!m_pFile || feof(m_pFile))
        return -1;

    int c = fgetc(m_pFile);
    if (c == '\n') {
        m_location.line++;
        m_location.col = 0;
    } else {
        m_location.col++;
    }
    return c;
}

inline void WaldLexer::ungetChar(int c)
{
    if (peekedChar >= 0)
        throw std::runtime_error("Can't push abck more than one char ...");

    peekedChar = c;
}
