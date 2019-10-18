#pragma once
#include <type_traits>

namespace stream {

// https://stackoverflow.com/questions/34099597/check-if-a-type-is-passed-in-variadic-template-parameter-pack
template <typename T, typename... Ts>
constexpr bool contains()
{
    return std::disjunction_v<std::is_same<T, Ts>...>;
}

template <typename T, typename S>
struct StronglyTypedAlias : public T {
};
}