#include "metrics/stopwatch.h"

namespace metrics {

template <typename Unit>
ScopedStopwatch<Unit>::ScopedStopwatch(Stopwatch<Unit>& parent)
    : m_parent(parent)
    , m_startTime(high_res_clock::now())
{
}

template <typename Unit>
ScopedStopwatch<Unit>::~ScopedStopwatch()
{
    auto now = high_res_clock::now();
    auto diff = now - m_startTime;
    size_t diffMicroSeconds = std::chrono::duration_cast<Unit>(diff).count();
    m_parent.m_value.fetch_add(diffMicroSeconds);
}

template <typename Unit>
Stopwatch<Unit>::Stopwatch(Stopwatch&& other) :
    m_value(other.m_value.load())
{
}

template <typename Unit>
ScopedStopwatch<Unit> Stopwatch<Unit>::getScopedStopwatch()
{
    return ScopedStopwatch(*this);
}

template <typename Unit>
Stopwatch<Unit>::operator nlohmann::json() const
{
    nlohmann::json json;
    json["type"] = "stopwatch";
    json["value"] = m_value.load();
    if constexpr (std::is_same_v<Unit, std::chrono::seconds>)
        json["unit"] = "seconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::milliseconds>)
        json["unit"] = "milliseconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::microseconds>)
        json["unit"] = "microseconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::nanoseconds>)
        json["unit"] = "nanoseconds";
    else
        assert(false); // static_assert always fails on clang-cl (6.0.1)
    return json;
}

template class Stopwatch<std::chrono::seconds>;
template class ScopedStopwatch<std::chrono::seconds>;
template class Stopwatch<std::chrono::milliseconds>;
template class ScopedStopwatch<std::chrono::milliseconds>;
template class Stopwatch<std::chrono::microseconds>;
template class ScopedStopwatch<std::chrono::microseconds>;
template class Stopwatch<std::chrono::nanoseconds>;
template class ScopedStopwatch<std::chrono::nanoseconds>;

}
