#pragma once
#include "metric.h"
#include <atomic>
#include <string_view>
#include <string>
#include <memory>

namespace metrics
{

class Histogram : public Metric {
public:
    Histogram(std::string_view unit, int start, int end, int numBins);
    ~Histogram() = default;

    void addItem(int value);

    operator nlohmann::json() const override final;
private:
    const int m_start;
    const int m_end;
    const float m_binSize;
    const int m_numBins;

    // Cant use a vector because atomics arent copy-constructable
    std::unique_ptr<std::atomic_size_t[]> m_bins;

    const std::string m_unit;
};

}