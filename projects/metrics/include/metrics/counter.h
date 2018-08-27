#pragma once
#include "metric.h"
#include <atomic>

namespace metrics
{

class Counter : public Metric {
public:
    Counter();
    ~Counter() = default;

    Counter& operator+=(int v);
    Counter& operator-=(int v);
    operator int() const;

    operator nlohmann::json() const override final;
private:
    std::atomic_int m_value;
};

}