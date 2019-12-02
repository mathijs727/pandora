#include "metrics/histogram.h"

namespace metrics {

Histogram::Histogram(std::string_view unit, int start, int end, int numBins)
    : m_start(start)
    , m_end(end)
    , m_binSize(static_cast<float>(end - start) / (numBins - 2))
    , m_numBins(numBins)
    , m_bins(std::make_unique<std::atomic_size_t[]>(numBins))
    , m_unit(unit)
{
    assert(numBins >= 3);
}

void Histogram::addItem(int value)
{
    int bin;
    if (value < m_start)
        bin = 0; // Casting to int truncates towards zero (up for negative numbers)
    else
        bin = std::min(m_numBins - 1, static_cast<int>((value - m_start) / m_binSize) + 1);
    m_bins[bin].fetch_add(1, std::memory_order::memory_order_relaxed);
}

Histogram::operator nlohmann::json() const
{
    std::vector<size_t> bins(m_numBins);
    for (int i = 0; i < m_numBins; i++)
        bins[i] = m_bins[i].load(std::memory_order::memory_order_relaxed);

    nlohmann::json json;
    json["type"] = "histogram";
    json["min"] = m_start;
    json["max"] = m_end;
    json["numBins"] = m_numBins;
    json["value"] = bins;
    json["unit"] = m_unit;
    return json;
}

}
