#include "metrics/counter.h"

namespace metrics {

template <typename T>
Counter<T>::Counter(std::string_view unit)
    : m_value(0), m_unit(unit)
{
}

template <typename T>
Counter<T>& Counter<T>::operator+=(T v)
{
    m_value.fetch_add(v);
    return *this;
}

template <typename T>
Counter<T>& Counter<T>::operator-=(T v)
{
    m_value.fetch_sub(v);
    return *this;
}

template <typename T>
Counter<T>::operator T() const
{
    return m_value.load();
}

template <typename T>
Counter<T>::operator nlohmann::json() const
{
    nlohmann::json json;
    json["type"] = "counter";
    json["value"] = m_value.load();
    json["unit"] = m_unit;
    return json;
}

template class Counter<int>;
template class Counter<unsigned>;
template class Counter<size_t>;

}
