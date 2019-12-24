#include "string_to_number.h"
#include "crack_atof.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <execution>
#include <gsl/span>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

template <typename T>
T fromString(std::string_view str)
{
    // <charconv> is fast and generic
    T value;
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

    // Faster than <charconv> on MSVC 19.23.28106.4 but not sure if it is a 100% correct
    return static_cast<float>(crackAtof(str));
}

template <>
double fromString(std::string_view str)
{
    /*// <charconv>
    double value;
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;*/

    // Faster than <charconv> on MSVC 19.23.28106.4 but not sure if it is a 100% correct
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
        while (cursor < stringSize && !isSpace(string[cursor]))
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
        while (cursor < string.size() && !isSpace(string[cursor]))
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

template <typename T>
pybind11::array_t<T> toNumpyArray(gsl::span<const T> items)
{
    pybind11::array_t<T> outArray { static_cast<pybind11::size_t>(items.size()) };
    for (int i = 0; i < items.size(); i++)
        outArray.mutable_at(i) = items[i];
    return outArray;
}

template <typename T>
pybind11::array_t<T> stringToNumpy(std::string_view string)
{
    //const char* chars = python::extract<const char*>(string);
    //int length = python::extract<int>(string.attr("__len__")());

    // Heap allocation so that it stays alive outside of this function
    std::vector<T> numbers;
    if (string.size() > 5 * 1024 * 1024) { // Use multi-threading if the string is over 5MB
        numbers = stringToVectorParallel<T>(string);
    } else {
        numbers = stringToVector<T>(string);
    }

    return toNumpyArray<T>(numbers);
}

template pybind11::array_t<float> stringToNumpy<float>(std::string_view string);
template pybind11::array_t<double> stringToNumpy<double>(std::string_view string);
template pybind11::array_t<int32_t> stringToNumpy<int32_t>(std::string_view string);
template pybind11::array_t<int64_t> stringToNumpy<int64_t>(std::string_view string);
template pybind11::array_t<uint32_t> stringToNumpy<uint32_t>(std::string_view string);
template pybind11::array_t<uint64_t> stringToNumpy<uint64_t>(std::string_view string);

