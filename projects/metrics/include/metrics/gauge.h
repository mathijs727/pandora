#pragma once
#include "metric.h"
#include <atomic>
#include <string_view>
#include <string>

namespace metrics
{

class Gauge : public Metric {
public:
    Gauge(std::string_view unit);
    ~Gauge() = default;

    Gauge& operator=(int v);
    operator int() const;

    operator nlohmann::json() const override final;
private:
    std::atomic_int m_value;
    const std::string m_unit;
};

}