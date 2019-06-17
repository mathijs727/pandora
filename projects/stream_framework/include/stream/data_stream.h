#pragma once
#include <atomic>
#include <gsl/gsl>
#include <vector>
#include <hpx/lcos/local/mutex.hpp>

namespace tasking {

template <typename T>
class DataStream {
public:
    bool isEmptyUnsafe() const;
    size_t sizeUnsafe() const;

    void push(std::vector<T>&& data);
    void push(gsl::span<const T> data);

    class Consumer {
    public:
        Consumer(std::vector<std::vector<T>>&& dataBlocks)
            : m_dataBlocks(std::move(dataBlocks))
        {
        }

        struct Iterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type = gsl::span<const T>;
            using difference_type = std::ptrdiff_t;
            using pointer = gsl::span<const T>*;
            using reference = gsl::span<const T>&;

            Iterator(gsl::span<const std::vector<T>> dataBlocks, size_t index = 0)
                : m_dataBlocks(dataBlocks)
                , m_index(index)
            {
            }

            bool operator!=(const Iterator& other) { return m_index != other.m_index; }
            Iterator& operator++() // ++iterator
            {
                m_index++;
                return *this;
            }
            Iterator operator++(int) // iterator++
            {
                return Iterator(m_dataBlocks, m_index++);
            }
            gsl::span<const T> operator*() { return m_dataBlocks[m_index]; }

        private:
            gsl::span<const std::vector<T>> m_dataBlocks;
            size_t m_index;
        };

        Iterator begin() const { return Iterator(m_dataBlocks, 0); }
        Iterator end() const { return Iterator(m_dataBlocks, m_dataBlocks.size()); }

    private:
        const std::vector<std::vector<T>> m_dataBlocks;
    };

    Consumer consume();

private:
    hpx::lcos::local::mutex m_mutex;
    std::atomic_size_t m_currentSize { 0 };
    std::vector<std::vector<T>> m_dataBlocks;
};

template <typename T>
bool DataStream<T>::isEmptyUnsafe() const
{
    return m_currentSize.load(std::memory_order::memory_order_relaxed) == 0;
}

template <typename T>
inline size_t DataStream<T>::sizeUnsafe() const
{
    return m_currentSize.load(std::memory_order::memory_order_relaxed);
}

template <typename T>
void DataStream<T>::push(std::vector<T>&& data)
{
    std::scoped_lock lock { m_mutex };
    m_currentSize += data.size();
    m_dataBlocks.push_back(std::move(data));
}

template <typename T>
void DataStream<T>::push(gsl::span<const T> data)
{
    std::scoped_lock lock { m_mutex };
    m_currentSize += data.size();

    std::vector<T> dataBlock(static_cast<size_t>(data.size()));
    std::copy(std::begin(data), std::end(data), std::begin(dataBlock));
    m_dataBlocks.push_back(std::move(dataBlock));
}

template <typename T>
inline typename DataStream<T>::Consumer DataStream<T>::consume()
{
    std::scoped_lock lock { m_mutex };
    m_currentSize = 0;
    return Consumer(std::move(m_dataBlocks));
}

}
