#pragma once
#include <string_view>

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
    inline bool operator==(const std::string_view other) const
    {
        return text == other;
    }
    inline bool operator!=(const std::string_view other) const
    {
        return text != other;
    }

    inline operator bool() const
    {
        return type != TokenType::NONE;
    }
};