#pragma once
#include <type_traits>

namespace pandora {

template <typename T>
struct Zero : std::false_type {
    static constexpr T get_value() { return {}; }
};

template <>
struct Zero<float> : std::true_type {
    static constexpr float get_value() { return 0.0f; }
};

template <>
struct Zero<double> : std::true_type {
    static constexpr double get_value() { return 0.0; }
};

template <>
struct Zero<int> : std::true_type {
    static constexpr int get_value() { return 0; }
};

template <typename T>
constexpr T zero()
{
    static_assert(Zero<T>::value, "Cannot instatiate unknown type with value zero");
    return Zero<T>::get_value();
}

template <typename T>
struct One : std::false_type {
    static constexpr T get_value() { return {}; }
};

template <>
struct One<float> : std::true_type {
    static constexpr float get_value() { return 1.0f; }
};

template <>
struct One<double> : std::true_type {
    static constexpr double get_value() { return 1.0; }
};

template <>
struct One<int> : std::true_type {
    static constexpr int get_value() { return 1; }
};

template <typename T>
constexpr T one()
{
    static_assert(One<T>::value, "Cannot instatiate unknown type with value one");
    return One<T>::get_value();
}

const float piF = 3.141592653589793f;
const double piD = 3.141592653589793;
}