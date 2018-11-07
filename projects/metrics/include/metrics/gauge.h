#pragma once
#include "metric.h"
#include <atomic>
#include <string_view>
#include <string>

namespace metrics
{

template <typename T = int>
class Gauge : public Metric {
public:
    Gauge(std::string_view unit);
    ~Gauge() = default;

    Gauge& operator=(T v);
    operator T() const;

    operator nlohmann::json() const override final;
private:
    std::atomic<T> m_value;
    const std::string m_unit;
};

}