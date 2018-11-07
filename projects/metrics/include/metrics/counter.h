#pragma once
#include "metric.h"
#include <atomic>
#include <string_view>
#include <string>

namespace metrics
{

template <typename T = int>
class Counter : public Metric {
public:
    Counter(std::string_view unit);
    ~Counter() = default;

    Counter<T>& operator++(int);
    Counter<T>& operator+=(T v);
    Counter<T>& operator-=(T v);
    operator T() const;

    operator nlohmann::json() const override final;
private:
    std::atomic<T> m_value;
    const std::string m_unit;
};

}