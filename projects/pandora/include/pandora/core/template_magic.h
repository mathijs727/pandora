#pragma once
#include <tuple>
#include <functional>

namespace pandora {

// Modified version of:
// https://stackoverflow.com/questions/1198260/iterate-over-tuple
// Answered by Marcin Zawiejski on 11 December 2017
template<
    size_t Index = 0,// Start iteration at 0 index
    typename TTuple, // The tuple
    size_t Size =
    std::tuple_size_v<
    std::remove_reference_t<TTuple>>,// Tuple size
    typename TCallable,
    typename... TArgs
>
auto accumulate_tuple(TTuple&& tuple, TCallable&& callable, TArgs&&... args)
{
    if constexpr (Index < Size)
    {
        auto res = std::invoke(callable, args..., std::get<Index>(tuple));

        if constexpr (Index + 1 < Size)
        {
            return res + accumulate_tuple<Index + 1>(
                std::forward<TTuple>(tuple),
                std::forward<TCallable>(callable),
                std::forward<TArgs>(args)...);
        }
        else {
            return res;
        }
    }
}

}