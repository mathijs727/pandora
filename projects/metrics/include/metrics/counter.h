#pragma once
#include "metric.h"
#include <atomic>

namespace metrics
{

class Counter : public Metric {
public:
    Counter(Identifier&& identifier);

    Counter& operator+=(int v);
    Counter& operator-=(int v);
    operator int() const;

private:
    std::atomic_int m_value;
};

}