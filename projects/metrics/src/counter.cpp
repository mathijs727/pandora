#include "metrics/counter.h"

namespace metrics {

Counter::Counter(Identifier&& identifier)
    : Metric(std::move(identifier))
    , m_value(0)
{
}

Counter& Counter::operator+=(int v)
{
    auto oldValue = m_value.fetch_add(v);
    auto newValue = oldValue + v;
    valueChanged(newValue);
    return *this;
}

Counter& Counter::operator-=(int v)
{
    auto oldValue = m_value.fetch_sub(v);
    auto newValue = oldValue - v;
    valueChanged(m_value);
    return *this;
}

Counter::operator int() const
{
    return m_value.load();
}

}
