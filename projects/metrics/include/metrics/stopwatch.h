#pragma once
#include "metrics/metric.h"
#include <chrono>

namespace metrics
{

template <typename Unit>
class Stopwatch;

template <typename Unit>
class ScopedStopwatch
{
public:
    ScopedStopwatch(Stopwatch<Unit>& parent);
    ~ScopedStopwatch();
private:
    Stopwatch<Unit>& m_parent;

    using high_res_clock = std::chrono::high_resolution_clock;
    high_res_clock::time_point m_startTime;
};

template <typename Unit>
class Stopwatch : public Metric
{
public:
    ScopedStopwatch<Unit> getScopedStopwatch();

    operator nlohmann::json() const override final;
private:
    friend class ScopedStopwatch<Unit>;
    size_t m_value = 0;
};

}