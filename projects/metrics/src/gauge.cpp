#include "metrics/gauge.h"

namespace metrics {

template <typename T>
Gauge<T>::Gauge(std::string_view unit)
    : m_value(0)
    , m_unit(unit)
{
}

template <typename T>
Gauge<T>& Gauge<T>::operator=(T v)
{
    m_value.store(v);
    return *this;
}

template <typename T>
Gauge<T>::operator T() const
{
    return m_value.load();
}

template <typename T>
Gauge<T>::operator nlohmann::json() const
{
    nlohmann::json json;
    json["type"] = "gauge";
    json["value"] = m_value.load();
    json["unit"] = m_unit;
    return json;
}

template class Gauge<int>;
template class Gauge<unsigned>;
template class Gauge<size_t>;


}
