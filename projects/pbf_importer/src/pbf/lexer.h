#pragma once
#include <cstddef>
#include <filesystem>
#include <span>
#include <mio/mmap.hpp>
#include <tuple>
#include <string_view>
#include <mio_cache_control/mmap.hpp>
#include <cstring>

namespace pbf {

class Lexer {
public:
    Lexer(std::filesystem::path filePath);

    template <typename T>
    T readT();
    template <typename T>
    std::span<const T> readArray(size_t numItems);

	bool endOfFile() const;

private:
    std::span<const std::byte> readBytes(size_t numBytes);

private:
    //mio::mmap_source m_mappedFile;
    mio_cache_control::mmap_sink m_mappedFile;
    std::span<const std::byte> m_fileBytes;

	const size_t m_fileSize;
    size_t m_cursor { 0 };
};

template <typename T>
inline T Lexer::readT()
{
    auto bytes = readBytes(sizeof(T));

    T out;
    std::memcpy(&out, bytes.data(), sizeof(T));
    return out;
}

template <>
inline std::string_view Lexer::readT()
{
    const int32_t size = readT<int32_t>();

    auto bytes = readBytes(size);
    const char* pBegin = reinterpret_cast<const char*>(bytes.data());
    return std::string_view(pBegin, size);
}

template <>
inline std::string Lexer::readT()
{
    return std::string(readT<std::string_view>());
}

template <>
inline std::filesystem::path Lexer::readT()
{
    const auto fileName = readT<std::string_view>();
    return std::filesystem::path(fileName);
}

template <typename T>
inline std::span<const T> Lexer::readArray(size_t numItems)
{
    auto bytes = readBytes(numItems * sizeof(T));
    const T* pItems = reinterpret_cast<const T*>(bytes.data());
    return std::span(pItems, numItems);
}

}