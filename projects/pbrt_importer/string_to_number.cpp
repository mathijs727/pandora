#include "string_to_number.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <string_view>
#include <cstdlib>
#include <cctype>

namespace python = boost::python;
namespace np = boost::python::numpy;

struct TokenResult
{
    std::string_view token;
    std::string_view next;
};
TokenResult getNextToken(std::string_view string) {
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
        string.substr(tokenEnd)
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

template <typename T>
std::vector<T> stringToVector(std::string_view string)
{
    // First pass to get the number of tokens (numbers)
    size_t count = 0;
    TokenResult tr = {
        {},
        string
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
        string
    };
    while (!tr.next.empty()) {
        tr = getNextToken(tr.next);
        if (!tr.token.empty())
        {
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
    std::vector<T>* numbers = new std::vector<T>(std::move(stringToVector<T>(std::string_view(chars, length))));

    // https://github.com/ndarray/Boost.NumPy/issues/28
    boost::python::handle<> h(::PyCapsule_New((void*)numbers->data(), nullptr, [](PyObject* obj) {
        auto* b = reinterpret_cast<std::vector<T>*>(PyCapsule_GetPointer(obj, nullptr));
        if (b) {
            std::cout << "DESTROYED" << std::endl;
            delete b;
        }
    }));

    np::dtype dt = np::dtype::get_builtin<T>();
    auto shape = python::make_tuple(numbers->size());
    auto stride = python::make_tuple(sizeof(T));

    return np::from_data(numbers->data(), dt, shape, stride, python::object(h));
}

template python::object stringToNumpy<float>(python::str string);

template python::object stringToNumpy<int>(python::str string);
