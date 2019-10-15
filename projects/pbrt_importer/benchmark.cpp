#include "src/crack_atof.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <execution>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

template <typename T>
T fromString(std::string_view str);

template <>
int fromString(std::string_view str)
{
    // <charconv> faster than atoi
    int value;
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;
}

template <>
float fromString(std::string_view str)
{
    /*// <charconv>
    float value;
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;*/

    // Faster than <charconv> on MSVC 19.23.28106.4
    return static_cast<float>(crackAtof(str));
}

template <>
double fromString(std::string_view str)
{
    /*// <charconv>
    double value;
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;*/

    // Faster than <charconv> on MSVC 19.23.28106.4
    return crackAtof(str);
}

constexpr std::array<bool, 256> compuateIsSpaceLUT()
{
    // https://en.cppreference.com/w/cpp/string/byte/isspace
    std::array<bool, 256> lut {};
    for (int c = 0; c < 256; c++)
        lut[c] = (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
    return lut;
}

inline bool isSpace(const char c)
{
    // Lookup table is faster than 6 comparisons
    static constexpr std::array<bool, 256> lut = compuateIsSpaceLUT();
    return lut[c];
    //return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

template <typename T>
std::vector<T> stringToVector(const std::string_view string)
{
    // Allocate
    std::vector<T> ret;

    const size_t stringSize = string.size();
    size_t cursor = 0;
    while (cursor < stringSize) {
        // std::isspace is slow because it tries to be too generic (checking locale settings)
        while (cursor < stringSize && isSpace(string[cursor]))
            cursor++;

        const size_t tokenStart = cursor;
        while (cursor < stringSize  && !isSpace(string[cursor]))
            cursor++;

        if (cursor == tokenStart)
            break;

        const std::string_view token = string.substr(tokenStart, cursor);
        ret.push_back(fromString<T>(token));
    }
    return ret;
}

template <typename T>
std::vector<T> stringToVectorParallel(const std::string_view string)
{
    const size_t numTasks = std::thread::hardware_concurrency() * 8;

    std::vector<std::string_view> blocks;
    size_t prevBlockEnd = 0;
    for (size_t i = 1; i < numTasks; i++) {
        size_t cursor = i * (string.size() / numTasks);
        while (!isSpace(string[cursor]) && cursor < string.size())
            cursor++;

        blocks.push_back(string.substr(prevBlockEnd, cursor - prevBlockEnd));
        prevBlockEnd = cursor;
    }
    blocks.push_back(string.substr(prevBlockEnd)); // Make sure final work block goes to end (no integer truncation issues)

    std::vector<std::vector<T>> partialResults { numTasks };
    std::transform(
        std::execution::par_unseq,
        std::begin(blocks),
        std::end(blocks),
        std::begin(partialResults),
        [](std::string_view partialStr) {
            return stringToVector<T>(partialStr);
        });

    size_t finalCount = 0;
    for (const auto& partialResult : partialResults)
        finalCount += partialResult.size();

    std::vector<T> finalResults;
    finalResults.resize(finalCount);
    size_t offset = 0;
    for (const auto& partialResult : partialResults) {
        // Slightly faster than std::copy
        std::memcpy(finalResults.data() + offset, partialResults.data(), partialResults.size());
        offset += partialResults.size();

        //std::copy(std::begin(partialResult), std::end(partialResult), std::back_inserter(finalResults));
    }
    return finalResults;
}

int main()
{
    std::string myString = "1000.0   ";
    for (int i = 0; i < 200 * 1000; i++)
        myString += " 1000.0";
    myString += "  ";

	std::cout << "Parsing string of " << myString.size() / 1000000.0f << "MB" << std::endl;

    using clock = std::chrono::high_resolution_clock;

    auto start = clock::now();
    auto result = stringToVector<double>(myString);
    auto end = clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Parsing took " << duration.count() << "ms" << std::endl;
    return 0;
}