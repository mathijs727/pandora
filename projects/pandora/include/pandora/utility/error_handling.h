#pragma once
#include <iostream>
#include <string_view>

namespace pandora {

#define THROW_ERROR(message) __THROW_ERROR(message, __FILE__, __LINE__)
inline void __THROW_ERROR(std::string_view errorMessage, std::string_view fileName, int line)
{
    std::cout << fileName << ":" << line << " ==> " << errorMessage << std::endl;
#ifdef _WIN32
    system("pause");
#endif
    throw std::runtime_error("");
}

inline void LOG_INFO(std::string_view infoMessage)
{
    std::cout << infoMessage << std::endl;
}

inline void LOG_WARNING(std::string_view warningMessage)
{
    std::cout << warningMessage << std::endl;
}

#define ALWAYS_ASSERT(...) __ALWAYS_ASSERT(__VA_ARGS__, __FILE__, __LINE__)
inline void __ALWAYS_ASSERT(bool value, std::string_view fileName, int line)
{
    if (!value) {
        __THROW_ERROR("", fileName, line);
    }
}

inline void __ALWAYS_ASSERT(bool value, std::string_view errorMessage, std::string_view fileName, int line)
{
    if (!value) {
        __THROW_ERROR(errorMessage, fileName, line);
    }
}

}
