#pragma once
#include "metric.h"
#include <atomic>
#include <string_view>
#include <string>

namespace metrics
{

class Counter : public Metric {
public:
    Counter(std::string_view unit);
    ~Counter() = default;

    Counter& operator+=(int v);
    Counter& operator-=(int v);
    operator int() const;

    operator nlohmann::json() const override final;
private:
    std::atomic_int m_value;
    const std::string m_unit;
};

}