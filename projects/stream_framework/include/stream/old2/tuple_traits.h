#pragma once
#include <tuple>
#include <type_traits>

template <typename T>
struct is_tuple : std::false_type {
};
template <typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {
};
template <typename T>
constexpr bool is_tuple_v = is_tuple<T>::value;

template <typename T>
struct make_tuple {
    using type = std::tuple<T>;
};
template <typename... Ts>
struct make_tuple<std::tuple<Ts...>> {
    using type = std::tuple<Ts...>;
};
template <typename T>
using make_tuple_t = typename make_tuple<T>::type;

// https://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type
template <typename T, typename... Us>
struct type_in : std::disjunction<std::is_same<T, Us>...> {
};
template <typename T, typename... Us>
constexpr bool type_in_v = type_in<T, Us...>::value;