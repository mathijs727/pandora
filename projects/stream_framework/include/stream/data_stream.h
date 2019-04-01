#pragma once
#include <array>
#include <gsl/gsl>
#include <list>
#include <mutex>
#include <mio/mmap.hpp>
#include <filesystem>

namespace tasking {

class DataStreamBlockImpl{
public:
    DataStreamBlockImpl(size_t maxSize);
    DataStreamBlockImpl(const DataStreamBlockImpl&) = delete;
    DataStreamBlockImpl(DataStreamBlockImpl&&) = default;
    ~DataStreamBlockImpl();

    void* getData()
    {
        return m_data;
    }

private:
    std::filesystem::path m_filePath;
    mio::mmap_sink m_memoryMapping;
    void* m_data;
};

template <typename T>
class DataStreamBlock {
public:
    DataStreamBlock(size_t maxSize);

    void push(T item);
    void push(gsl::span<const T> items);

    size_t spaceLeft() const;
    bool full() const;

    gsl::span<const T> getData() const;

private:
    T* m_start;
    T* m_end;
    T* m_current;

    DataStreamBlockImpl m_implementation;
};

template <typename T>
class DataStream {
public:
    DataStream(size_t blockSize);

    void push(T item);
    void push(gsl::span<const T> items);

    bool isEmpty() const;
    const DataStreamBlock<T> getBlock();

private:
    const size_t m_blockSize;
    std::list<DataStreamBlock<T>> m_blocks;
    mutable std::mutex m_mutex;
};

template <typename T>
inline DataStream<T>::DataStream(size_t blockSize)
    : m_blockSize(blockSize)
{
}

template <typename T>
inline void DataStream<T>::push(T item)
{
    std::scoped_lock<std::mutex> l(m_mutex);

    if (m_blocks.empty() || m_blocks.front().spaceLeft() == 0)
        m_blocks.emplace_front(m_blockSize);

    m_blocks.front().push(item);
}

template <typename T>
inline void DataStream<T>::push(gsl::span<const T> items)
{
    std::scoped_lock<std::mutex> l(m_mutex);

    std::ptrdiff_t itemsPushed = 0;
    while (itemsPushed < items.size()) {
        if (m_blocks.empty() || m_blocks.front().full())
            m_blocks.emplace_front(m_blockSize);

        auto itemsToPush = std::min(static_cast<std::ptrdiff_t>(m_blocks.front().spaceLeft()), items.size() - itemsPushed);
        m_blocks.front().push(items.subspan(itemsPushed, itemsToPush));
        itemsPushed += itemsToPush;
    }
}

template <typename T>
inline bool DataStream<T>::isEmpty() const
{
    std::scoped_lock<std::mutex> l(m_mutex);

    return m_blocks.empty();
}

template <typename T>
inline const DataStreamBlock<T> DataStream<T>::getBlock()
{
    std::scoped_lock<std::mutex> l(m_mutex);

    auto tmp = std::move(m_blocks.back());
    m_blocks.pop_back();
    return tmp;
}

template <typename T>
inline DataStreamBlock<T>::DataStreamBlock(size_t maxSize)
    : m_implementation(maxSize * sizeof(T))
{
    m_current = m_start = reinterpret_cast<T*>(m_implementation.getData());
    m_end = m_start + maxSize;
}

template <typename T>
inline void DataStreamBlock<T>::push(T item)
{
    assert(m_current < m_end);
    *(m_current++) = item;
}

template <typename T>
inline void DataStreamBlock<T>::push(gsl::span<const T> items)
{
    assert(sizeof(items) <= spaceLeft());

    std::copy(std::begin(items), std::end(items), m_current);
    m_current += items.size();
}

template <typename T>
inline gsl::span<const T> DataStreamBlock<T>::getData() const
{
    return gsl::span<const T>(m_start, m_current);
}

template <typename T>
inline size_t DataStreamBlock<T>::spaceLeft() const
{
    return std::distance(m_current, m_end);
}

template <typename T>
inline bool DataStreamBlock<T>::full() const
{
    return m_current >= m_end;
}

}