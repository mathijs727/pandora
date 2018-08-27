#include "metrics/counter.h"

namespace metrics {

Counter::Counter(std::string_view unit)
    : m_value(0), m_unit(unit)
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
    json["unit"] = m_unit;
    return json;
}

}
