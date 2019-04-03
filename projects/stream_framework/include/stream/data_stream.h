#pragma once
#include <atomic>
#include <gsl/gsl>
#include <mutex>
#include <vector>

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
        struct Iterator {
            using iterator_category = std::forward_iterator_tag;
            using value_type = gsl::span<const T>;
            using difference_type = std::ptrdiff_t;
            using pointer = gsl::span<const T>*;
            using reference = gsl::span<const T>&;

            bool operator!=(const Iterator& other) { return m_index != other.m_index; }
            bool operator++() { m_index++; }
            gsl::span<const T> operator*() { return m_dataBlocks[m_index]; }

        private:
            Iterator(gsl::span<const std::vector<T>> dataBlocks, int index = 0)
                : m_dataBlocks(dataBlocks)
                , m_index(index)
            {
            }

        private:
            gsl::span<const std::vector<T>> m_dataBlocks;
            int m_index;
        };

        Iterator begin() const { return Iterator(m_dataBlocks, m_dataBlocks.size()); }
        Iterator end() const { return Iterator(m_dataBlocks, m_dataBlocks.size()); }

    private:
        Consumer(std::vector<const std::vector<T>>&& dataBlocks)
            : m_dataBlocks(std::move(dataBlocks))
        {
        }

    private:
        const std::vector<const std::vector<T>> m_dataBlocks;
    };

    Consumer consume();

private:
    std::mutex m_mutex;
    std::atomic_size_t m_currentSize { 0 };
    std::vector<const std::vector<T>> m_dataBlocks;
};

template <typename T>
bool DataStream<T>::isEmptyUnsafe() const
{
    return m_currentSize.load() == 0;
}

template <typename T>
inline size_t DataStream<T>::sizeUnsafe() const
{
    return m_currentSize.load();
}

template <typename T>
void DataStream<T>::push(std::vector<T>&& data)
{
    m_currentSize += data.size();
    m_dataBlocks.push_back(std::move(data));
}

template <typename T>
void DataStream<T>::push(gsl::span<const T> data)
{
    m_currentSize += data.size();

    std::vector<T> dataOwner(static_cast<size_t>(data.size()));
    std::copy(std::begin(data), std::end(data), std::begin(dataOwner));
    m_data.emplace(std::move(dataOwner));
}

template <typename T>
inline typename DataStream<T>::Consumer DataStream<T>::consume()
{
    std::scoped_lock lock { m_mutex };
    m_currentSize = 0;
    return Consumer(std::move(m_dataBlocks));
}

}
