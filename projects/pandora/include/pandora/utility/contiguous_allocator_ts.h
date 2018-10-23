#pragma once
#include "pandora/flatbuffers/contiguous_allocator_generated.h"
#include "pandora/utility/error_handling.h"
#include <atomic>
#include <cassert>
#include <fstream>
#include <memory>
#include <mio/mmap.hpp>
#include <string_view>
#include <tbb/enumerable_thread_specific.h>
#include <thread>
#include <tuple>
#include <unordered_map>

namespace pandora {

template <typename T>
class ContiguousAllocatorTS {
public:
    ContiguousAllocatorTS(ContiguousAllocatorTS&& other);
    ContiguousAllocatorTS(uint32_t maxSize, uint32_t blockSize = 4096);
    ContiguousAllocatorTS(const serialization::ContiguousAllocator* serializedAllocator);

    using Handle = uint32_t;
    template <typename... Args>
    std::pair<Handle, T*> allocate(Args&&... args);

    template <typename... Args>
    std::pair<Handle, T*> allocateN(unsigned N, Args&&... args);

    template <typename InitF>
    std::pair<Handle, T*> allocateNInitF(unsigned N, InitF&& f);

    inline T& get(Handle handle) const { return reinterpret_cast<T&>(m_start[handle]); };

    size_t size() const { return m_currentSize.load(); };
    size_t sizeBytes() const { return m_currentSize.load() * sizeof(T); };

    void compact();

    flatbuffers::Offset<serialization::ContiguousAllocator> serialize(flatbuffers::FlatBufferBuilder& builder) const;

private:
    uint32_t allocateBlock();

private:
    uint32_t m_maxSize;
    const uint32_t m_blockSize;

    struct alignas(std::alignment_of_v<T>) EmptyItem {
    private:
        std::byte __padding[sizeof(T)];
    };
    std::unique_ptr<EmptyItem[]> m_start;
    std::atomic_uint32_t m_currentSize;

    struct ThreadLocalData {
        bool isInitialized = false;
        uint32_t index = 0;
        uint32_t space = 0;
    };
    tbb::enumerable_thread_specific<ThreadLocalData> m_threadLocalBlocks;
};

template <typename T>
inline ContiguousAllocatorTS<T>::ContiguousAllocatorTS(ContiguousAllocatorTS&& other)
    : m_maxSize(other.m_maxSize)
    , m_blockSize(other.m_blockSize)
    , m_start(std::move(other.m_start))
    , m_currentSize(other.m_currentSize.load())
    , m_threadLocalBlocks(std::move(other.m_threadLocalBlocks))
{
}

template <typename T>
inline ContiguousAllocatorTS<T>::ContiguousAllocatorTS(uint32_t maxSize, uint32_t blockSize)
    : m_maxSize((uint32_t)std::thread::hardware_concurrency() * std::min(maxSize, blockSize) + maxSize)
    , m_blockSize(std::min(maxSize, blockSize))
    , m_start(new EmptyItem[m_maxSize])
    , m_currentSize(0)
{
    ALWAYS_ASSERT(m_maxSize > 0);
    ALWAYS_ASSERT(m_blockSize > 0);
}

template <typename T>
inline ContiguousAllocatorTS<T>::ContiguousAllocatorTS(const serialization::ContiguousAllocator* serializedAllocator)
    : m_maxSize(serializedAllocator->maxSize())
    , m_blockSize(serializedAllocator->blockSize())
    , m_start(new EmptyItem[serializedAllocator->maxSize()])
    , m_currentSize(serializedAllocator->currentSize())
{
    std::memcpy(m_start.get(), serializedAllocator->data()->Data(), m_currentSize * sizeof(T));
}

template <typename T>
template <typename... Args>
inline std::pair<typename ContiguousAllocatorTS<T>::Handle, T*> ContiguousAllocatorTS<T>::allocate(Args&&... args)
{
    return allocateN(1, std::forward<Args>(args)...);
}

template <typename T>
template <typename... Args>
inline std::pair<typename ContiguousAllocatorTS<T>::Handle, T*> ContiguousAllocatorTS<T>::allocateN(unsigned N, Args&&... args)
{
    assert(N <= m_blockSize);

    auto& currentBlock = m_threadLocalBlocks.local();
    if (currentBlock.space < N) {
        currentBlock.isInitialized = true;
        currentBlock.index = allocateBlock();
        currentBlock.space = m_blockSize;
    }

    uint32_t resultIndex = currentBlock.index;
    currentBlock.index += N;
    currentBlock.space -= N;

    T* dataPtr = reinterpret_cast<T*>(m_start.get() + resultIndex);
    for (uint32_t i = 0; i < N; i++)
        new (dataPtr + i) T(std::forward<Args>(args)...);

    return { resultIndex, dataPtr };
}

template <typename T>
template <typename InitF>
inline std::pair<typename ContiguousAllocatorTS<T>::Handle, T*> ContiguousAllocatorTS<T>::allocateNInitF(unsigned N, InitF&& initFunc)
{
    assert(N <= m_blockSize);

    auto& currentBlock = m_threadLocalBlocks.local();
    if (currentBlock.space < N) {
        currentBlock.isInitialized = true;
        currentBlock.index = allocateBlock();
        currentBlock.space = m_blockSize;
    }

    uint32_t resultIndex = currentBlock.index;
    currentBlock.index += N;
    currentBlock.space -= N;

    T* dataPtr = reinterpret_cast<T*>(m_start.get() + resultIndex);
    for (uint32_t i = 0; i < N; i++)
        new (dataPtr + i) T(initFunc(i));

    return { resultIndex, dataPtr };
}

template <typename T>
inline uint32_t ContiguousAllocatorTS<T>::allocateBlock()
{
    uint32_t index = m_currentSize.fetch_add(m_blockSize);
    if (index + m_blockSize > m_maxSize)
        THROW_ERROR("ContiguousAllocator ran out of memory!");
    return index;
}

template <typename T>
inline void ContiguousAllocatorTS<T>::compact()
{
    // Determine the actual current size (substracting any empty space in thread local blocks)
    std::unordered_map<uint32_t, uint32_t> threadLocalBlocks;
    for (const auto& block : m_threadLocalBlocks) {
        if (block.isInitialized) {
            uint32_t itemsInBlock = (m_blockSize - block.space);
            uint32_t startOfBlock = block.index - itemsInBlock;
            threadLocalBlocks.insert({ startOfBlock, itemsInBlock });
        }
    }

    // Move the data per block. We need to take care of blocks that are not completely filled.
    auto compactedData = std::make_unique<EmptyItem[]>(m_currentSize);
    for (uint32_t blockStart = 0; blockStart < m_currentSize; blockStart += m_blockSize) {
        if (auto iter = threadLocalBlocks.find(blockStart); iter != threadLocalBlocks.end()) {
            // Partially filled block
            for (uint32_t i = blockStart; i < blockStart + iter->second; i++) {
                // Call the move constructor
                auto newAddress = reinterpret_cast<T*>(&compactedData[i]);
                auto& oldValue = reinterpret_cast<T&>(m_start[i]);
                new (newAddress) T(std::move(oldValue));
            }
        } else {
            // Fully filled block
            uint32_t blockEnd = blockStart + m_blockSize;
            for (uint32_t i = blockStart; i < blockEnd; i++) {
                // Call the move constructor
                auto newAddress = reinterpret_cast<T*>(&compactedData[i]);
                auto& oldValue = reinterpret_cast<T&>(m_start[i]);
                new (newAddress) T(std::move(oldValue));
            }
        }
    }

    m_start = std::move(compactedData);
    m_maxSize = m_currentSize;
}

template <typename T>
inline flatbuffers::Offset<serialization::ContiguousAllocator> ContiguousAllocatorTS<T>::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    auto serializedData = builder.CreateVector(reinterpret_cast<const int8_t*>(m_start.get()), m_currentSize * sizeof(T));
    return serialization::CreateContiguousAllocator(builder, m_maxSize, m_blockSize, m_currentSize, serializedData);
}

}
