#pragma once
#include "data_channel.h"
#include <memory>
#include <optional>

namespace tasking {

template <typename T>
class StreamConsumer {
public:
    StreamConsumer(std::shared_ptr<DataChannel<T>> pChannel, MemoryBlockAllocator* pAllocator);
    ~StreamConsumer();

    bool loadData();
    std::span<T> data();
    std::span<const T> data() const;

private:
    std::shared_ptr<DataChannel<T>> m_pChannel;
    MemoryBlockAllocator* m_pBlockAllocator;
    std::optional<DataBlock<T>> m_currentDataBlock;
};

template <typename T>
class StreamProducer {
public:
    StreamProducer(std::shared_ptr<DataChannel<T>> pChannel, MemoryBlockAllocator* pAllocator);
    ~StreamProducer(); // Flush

    // Internally keep a bucket and once bucket is full submit it.
    void push(T&& item);
    void push(std::span<const T> items);

    void flush();

private:
    std::shared_ptr<DataChannel<T>> m_pChannel;
    MemoryBlockAllocator* m_pBlockAllocator;
    std::optional<DataBlock<T>> m_currentDataBlock;
};

template <typename T>
inline StreamProducer<T>::StreamProducer(std::shared_ptr<DataChannel<T>> pChannel, MemoryBlockAllocator* pAllocator)
    : m_pChannel(pChannel)
    , m_pBlockAllocator(pAllocator)

{
}

template <typename T>
inline StreamProducer<T>::~StreamProducer()
{
    flush();
}

template <typename T>
inline void StreamProducer<T>::push(T&& item)
{
    if (!m_currentDataBlock) {
        m_currentDataBlock = DataBlock<T>(m_pBlockAllocator->allocate(1));
    }

    if (m_currentDataBlock->full()) {
        m_pChannel->pushBlocking(std::move(*m_currentDataBlock));
        m_currentDataBlock = DataBlock<T>(m_pBlockAllocator->allocate(1));
    }

    m_currentDataBlock->push(std::move(item));
}

template <typename T>
inline void StreamProducer<T>::push(std::span<const T> items)
{
    // TODO: make more efficient by doing a memcpy (if trivially copyable)
    for (const T& item : items)
        push(std::move(item));
}

template <typename T>
inline void StreamProducer<T>::flush()
{
    if (m_currentDataBlock && !m_currentDataBlock->empty()) {
        m_pChannel->pushBlocking(std::move(*m_currentDataBlock));
        m_currentDataBlock.reset();
    }
}

template <typename T>
inline StreamConsumer<T>::StreamConsumer(std::shared_ptr<DataChannel<T>> pChannel, MemoryBlockAllocator* pAllocator)
    : m_pChannel(pChannel)
    , m_pBlockAllocator(pAllocator)
{
}

template <typename T>
inline StreamConsumer<T>::~StreamConsumer()
{
    if (m_currentDataBlock)
        m_pBlockAllocator->deallocate(m_currentDataBlock->pMemoryBlock, 1);
}

template <typename T>
inline bool StreamConsumer<T>::loadData()
{
    if (m_currentDataBlock)
        m_pBlockAllocator->deallocate(m_currentDataBlock->pMemoryBlock, 1);
    m_currentDataBlock = m_pChannel->tryPop();
    return m_currentDataBlock.has_value();
}

template <typename T>
inline std::span<T> StreamConsumer<T>::data()
{
    assert(m_currentDataBlock.has_value());
    return m_currentDataBlock->data();
}

template <typename T>
inline std::span<const T> StreamConsumer<T>::data() const
{
    assert(m_currentDataBlock.has_value());
    return m_currentDataBlock->data();
}

}