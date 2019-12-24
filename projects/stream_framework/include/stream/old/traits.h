#pragma once
#include <type_traits>

namespace tasking {

// https://stackoverflow.com/questions/16252902/sfinae-set-of-types-contains-the-type
template <typename T, typename...>
struct is_contained : std::false_type {
};

template <typename T, typename Head, typename... Tail>
struct is_contained<T, Head, Tail...> : std::integral_constant<bool,
                                            std::is_same<T, Head>::value || is_contained<T, Tail...>::value> {
};

}
