#pragma once
#include <array>
#include <atomic>
#include <filesystem>
#include <gsl/gsl>
#include <list>
#include <mio/mmap.hpp>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>

namespace tasking {

class DataStreamChunkImpl {
public:
    DataStreamChunkImpl(size_t maxSize);
    DataStreamChunkImpl(const DataStreamChunkImpl&) = delete;
    DataStreamChunkImpl(DataStreamChunkImpl&&) = default;
    ~DataStreamChunkImpl();

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
class DataStreamChunk {
public:
    DataStreamChunk(size_t maxSize);

    gsl::span<T> reserveForWriting(size_t numItems);

    size_t spaceLeft() const;
    bool full() const;

    gsl::span<const T> getData() const;

private:
    T* m_start;
    T* m_end;
    T* m_current;

    DataStreamChunkImpl m_implementation;
};

template <typename T>
class DataStream {
public:
    DataStream(size_t chunkSize);
    DataStream(DataStream&&) = delete;
    DataStream(const DataStream&) = delete;
    ~DataStream() = default;

    struct WriteOp {
    public:
        gsl::span<T> data;

        ~WriteOp();

    private:
        friend class DataStream<T>;
        WriteOp(gsl::span<T> data, std::atomic_int& writeCount);

        std::atomic_int& m_writeCount;
    };
    WriteOp reserveRangeForWriting(size_t numItems)
    {
        if (numItems > m_chunkSize) {
            spdlog::critical("Trying to add to many items ({}) at once to a data stream (of size {})", numItems, m_chunkSize);
            exit(1);
        }

        std::scoped_lock<std::mutex> l(m_mutex);

        if (m_chunksAndWriteCounts.empty() || std::get<0>(m_chunksAndWriteCounts.front()).spaceLeft() < numItems)
            m_chunksAndWriteCounts.emplace_front(m_chunkSize, 0);

        auto& [chunk, writeCount] = m_chunksAndWriteCounts.front();
        writeCount++;
        return WriteOp(chunk.reserveForWriting(numItems), writeCount);
    }

    bool isEmpty() const;
    std::optional<const DataStreamChunk<T>> popChunk();

private:
    const size_t m_chunkSize;
    std::list<std::tuple<DataStreamChunk<T>, std::atomic_int>> m_chunksAndWriteCounts;
    mutable std::mutex m_mutex;
};

template <typename T>
inline DataStream<T>::DataStream(size_t chunkSize)
    : m_chunkSize(chunkSize)
{
}

/*template <typename T>
inline void DataStream<T>::push(T item)
{
    std::scoped_lock<std::mutex> l(m_mutex);

    if (m_chunks.empty() || m_chunks.front().spaceLeft() == 0)
        m_chunks.emplace_front(m_chunkSize);

    m_chunks.front().push(item);
}

template <typename T>
inline void DataStream<T>::push(gsl::span<const T> items)
{
    std::scoped_lock<std::mutex> l(m_mutex);

    std::ptrdiff_t itemsPushed = 0;
    while (itemsPushed < items.size()) {
        if (m_chunks.empty() || m_chunks.front().full())
            m_chunks.emplace_front(m_chunkSize);

        auto itemsToPush = std::min(static_cast<std::ptrdiff_t>(m_chunks.front().spaceLeft()), items.size() - itemsPushed);
        m_chunks.front().push(items.subspan(itemsPushed, itemsToPush));
        itemsPushed += itemsToPush;
    }
}*/

template <typename T>
inline DataStream<T>::WriteOp::WriteOp(gsl::span<T> data, std::atomic_int& writeCount)
    : data(data)
    , m_writeCount(writeCount)
{
}

template <typename T>
inline DataStream<T>::WriteOp::~WriteOp()
{
    m_writeCount--;
}

template <typename T>
inline bool DataStream<T>::isEmpty() const
{
    std::scoped_lock<std::mutex> l(m_mutex);

    return m_chunksAndWriteCounts.empty();
}

template <typename T>
inline std::optional<const DataStreamChunk<T>> DataStream<T>::popChunk()
{
    std::scoped_lock<std::mutex> l(m_mutex);

    for (auto iter = std::begin(m_chunksAndWriteCounts); iter != std::end(m_chunksAndWriteCounts); iter++) {
        auto& [chunkRef, writeCount] = *iter;
        if (writeCount == 0) {
            auto chunk = std::move(chunkRef);
            m_chunksAndWriteCounts.erase(iter);
            return { std::move(chunk) };
        }
    }

    return { };
}

template <typename T>
inline DataStreamChunk<T>::DataStreamChunk(size_t maxSize)
    : m_implementation(maxSize * sizeof(T))
{
    m_current = m_start = reinterpret_cast<T*>(m_implementation.getData());
    m_end = m_start + maxSize;
}

template <typename T>
inline gsl::span<T> DataStreamChunk<T>::reserveForWriting(size_t numItems)
{
    assert(numItems <= spaceLeft());

    auto ret = gsl::make_span(m_current, m_current + numItems);
    m_current += numItems;
    return ret;
}

/*template <typename T>
inline void DataStreamChunk<T>::push(T item)
{
    assert(m_current < m_end);
    *(m_current++) = item;
}

template <typename T>
inline void DataStreamChunk<T>::push(gsl::span<const T> items)
{
    assert(sizeof(items) <= spaceLeft());

    std::copy(std::begin(items), std::end(items), m_current);
    m_current += items.size();
}*/

template <typename T>
inline gsl::span<const T> DataStreamChunk<T>::getData() const
{
    return gsl::span<const T>(m_start, m_current);
}

template <typename T>
inline size_t DataStreamChunk<T>::spaceLeft() const
{
    return std::distance(m_current, m_end);
}

template <typename T>
inline bool DataStreamChunk<T>::full() const
{
    return m_current >= m_end;
}
}
