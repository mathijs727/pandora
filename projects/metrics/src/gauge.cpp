#include "metrics/gauge.h"

namespace metrics {

Gauge::Gauge(std::string_view unit)
    : m_value(0)
    , m_unit(unit)
{
}

Gauge& Gauge::operator=(int v)
{
    m_value.store(v);
    return *this;
}

Gauge::operator int() const
{
    return m_value.load();
}

Gauge::operator nlohmann::json() const
{
    nlohmann::json json;
    json["type"] = "gauge";
    json["value"] = m_value.load();
    json["unit"] = m_unit;
    return json;
}

}
