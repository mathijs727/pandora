#include "metrics/stopwatch.h"
#include "..\include\metrics\stopwatch.h"

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
    m_parent.m_value = diffMicroSeconds;
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
    json["value"] = m_value;
    if constexpr (std::is_same_v<Unit, std::chrono::seconds>)
        json["unit"] = "seconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::milliseconds>)
        json["unit"] = "milliseconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::microseconds>)
        json["unit"] = "microseconds";
    else if constexpr (std::is_same_v<Unit, std::chrono::nanoseconds>)
        json["unit"] = "nanoseconds";
    else
        static_assert(false, "Unknown time unit");
    return json;
}

template Stopwatch<std::chrono::seconds>;
template ScopedStopwatch<std::chrono::seconds>;
template Stopwatch<std::chrono::milliseconds>;
template ScopedStopwatch<std::chrono::milliseconds>;
template Stopwatch<std::chrono::microseconds>;
template ScopedStopwatch<std::chrono::microseconds>;
template Stopwatch<std::chrono::nanoseconds>;
template ScopedStopwatch<std::chrono::nanoseconds>;

}
