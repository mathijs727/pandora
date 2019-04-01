#pragma once
#include <gsl/gsl>
#include <vector>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_queue.h>

namespace tasking {

template <typename T>
class DataStream {
public:
    bool isEmpty() const;
    void push(gsl::span<const T> data);
    std::vector<std::vector<T>> consume();

private:
    tbb::concurrent_queue<std::vector<T>> m_data;
};

template <typename T>
bool DataStream<T>::isEmpty() const
{
    return m_data.empty();
}

template <typename T>
void DataStream<T>::push(gsl::span<const T> data)
{
    SPDLOG_TRACE("Performance warning: DataStream<T>::push(gsl::span<const T>) requires data copy (moving a vector does not)");

    std::vector<T> dataOwner(static_cast<size_t>(data.size()));
    std::copy(std::begin(data), std::end(data), std::begin(dataOwner));
    m_data.emplace(std::move(dataOwner));
}

template <typename T>
inline std::vector<std::vector<T>> DataStream<T>::consume()
{
    std::vector<std::vector<T>> ret;

    std::vector<T> data;
    while (m_data.try_pop(data))
        ret.emplace_back(std::move(data));

    return ret;
}

}
