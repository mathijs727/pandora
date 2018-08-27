#include "metrics/counter.h"

namespace metrics {

Counter::Counter()
    : m_value(0)
{
}

Counter& Counter::operator+=(int v)
{
    m_value.fetch_add(v);
    return *this;
}

Counter& Counter::operator-=(int v)
{
    m_value.fetch_sub(v);
    return *this;
}

Counter::operator int() const
{
    return m_value.load();
}

Counter::operator nlohmann::json() const
{
    nlohmann::json json;
    json["type"] = "counter";
    json["value"] = m_value.load();
    return json;
}

}
