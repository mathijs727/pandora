#include "string_to_number.h"
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>
#include <tuple>

namespace python = boost::python;
namespace np = boost::python::numpy;

struct TokenResult {
    std::string_view token;
    std::string_view next;
    int charsSkipped;
};
TokenResult getNextToken(std::string_view string)
{
    int tokenStart = 0;
    while (tokenStart < string.size() && std::isspace(string[tokenStart])) {
        tokenStart++;
    }

    if (tokenStart == string.size())
        return { {}, {} };

    int tokenEnd = tokenStart;
    while (tokenEnd < string.size() && !std::isspace(string[tokenEnd])) {
        tokenEnd++;
    }

    return {
        string.substr(tokenStart, tokenEnd - tokenStart),
        string.substr(tokenEnd),
        tokenEnd
    };
}

template <typename T>
T fromString(const char* t);

template <>
int fromString(const char* str)
{
    return std::atoi(str);
}

template <>
float fromString(const char* str)
{
    char* dummy;
    return std::strtof(str, &dummy);
}

template <>
double fromString(const char* str)
{
    return std::atof(str);
}

template <typename T>
std::vector<T> stringToVectorThreaded(std::string_view string)
{
    unsigned numThreads = std::thread::hardware_concurrency();
    int approxPartSize = static_cast<int>(string.size() / numThreads);

    // First pass to get the number of tokens (numbers)
    std::vector<std::tuple<int, std::string_view>> parts;
    TokenResult tr = {
        {},
        string,
        0
    };
    int count = 0;
    int currentChar = 0;
    int partStartCount = 0;
    int partStartChar = 0;
    while (!tr.next.empty()) {
        tr = getNextToken(tr.next);
        if (tr.token.empty())
            break;

        count++;
        currentChar += tr.charsSkipped;

        if (currentChar - partStartChar > approxPartSize) {
            parts.push_back({ partStartCount, std::string_view(string.data() + partStartChar, currentChar - partStartChar) });
            partStartCount = count;
            partStartChar = currentChar;
        }
    }
    if (count != partStartCount) {
        parts.push_back({ partStartCount, std::string_view(string.data() + partStartChar, currentChar - partStartChar) });
    }

    //std::cout << "Desired thread count: " << numThreads << std::endl;
    //std::cout << "Actual thread count: " << parts.size() << std::endl;

    // Allocate
    std::vector<T> ret(count);

    // Second pass
    std::vector<std::thread> threadPool;
    for (const auto [partStartIndex, partString] : parts) {
        threadPool.push_back(std::thread([partStartIndex, partString, &ret]() {
            int outIndex = partStartIndex;
            TokenResult tr = {
                {},
                partString,
                0
            };
            while (!tr.next.empty()) {
                tr = getNextToken(tr.next);
                if (!tr.token.empty()) {
                    ret[outIndex++] = fromString<T>(tr.token.data());
                }
            }
        }));
    }

    for (auto& thread : threadPool)
        thread.join();

    return ret;
}

template <typename T>
std::vector<T> stringToVector(std::string_view string)
{
    // First pass to get the number of tokens (numbers)
    size_t count = 0;
    TokenResult tr = {
        {},
        string,
        0
    };
    while (!tr.next.empty()) {
        tr = getNextToken(tr.next);
        if (!tr.token.empty())
            count++;
    }

    // Allocate
    std::vector<T> ret;
    ret.reserve(count);

    // Second pas
    tr = {
        {},
        string,
        0
    };
    while (!tr.next.empty()) {
        tr = getNextToken(tr.next);
        if (!tr.token.empty()) {
            ret.push_back(fromString<T>(tr.token.data()));
        }
    }

    return ret;
}

template <typename T>
python::object stringToNumpy(python::str string)
{
    const char* chars = python::extract<const char*>(string);
    int length = python::extract<int>(string.attr("__len__")());

    // Heap allocation so that it stays alive outside of this function
    std::vector<T>* numbers;
    if (length > 10 * 1000 * 1000) { // Use multi-threading if the string is over 10MB
        numbers = new std::vector<T>(std::move(stringToVectorThreaded<T>(std::string_view(chars, length))));
    } else {
        numbers = new std::vector<T>(std::move(stringToVector<T>(std::string_view(chars, length))));
    }

    // https://github.com/ndarray/Boost.NumPy/issues/28
    boost::python::handle<> h(::PyCapsule_New((void*)numbers, "numpy_array_from_string", [](PyObject* obj) {
        auto* b = reinterpret_cast<std::vector<T>*>(PyCapsule_GetPointer(obj, "numpy_array_from_string"));
        if (b) {
            delete b;
        }
    }));

    np::dtype dt = np::dtype::get_builtin<T>();
    auto shape = python::make_tuple(numbers->size());
    auto stride = python::make_tuple(sizeof(T));

    return np::from_data(numbers->data(), dt, shape, stride, python::object(h));
}

template python::object stringToNumpy<int>(python::str string);
template python::object stringToNumpy<float>(python::str string);
template python::object stringToNumpy<double>(python::str string);
