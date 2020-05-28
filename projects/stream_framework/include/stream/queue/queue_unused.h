#pragma once
#include "system.h"
#include <EASTL/fixed_vector.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <gsl/gsl>
#include <tbb/concurrent_queue.h>
#include <tbb/scalable_allocator.h>
#include <tbb/task_arena.h>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>

namespace tasking {

template <size_t Size>
struct alignas(64) MemoryBlock {
    std::byte memory[Size];
};

template <typename T, size_t ChunkSize>
class MPSCQueue {
private:
    struct Chunk;

public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = gsl::span<const T>;

    private:
        friend class MPSCQueue;
        Iterator(Chunk* pChunk, size_t index)
            : pChunk(pChunk)
            , index(index)
        {
        }

        Chunk* pChunk;
        size_t index;
    };

public:
    MPSCQueue(tbb::scalable_allocator<MemoryBlock<ChunkSize>>& allocator);

    void push(const T& item);
    void push(gsl::span<const T> items);

    size_t approxQueueSize() const;
    void forwardThreadLocalChunksUnsafe();

	Iterator();

private:
    void forwardThreadLocalChunk(int threadIndex);

    struct alignas(cacheline_size) Chunk;
    Chunk* allocateChunk();
    void deallocateChunk(Chunk* pChunk);

private:
    struct alignas(cacheline_size) Chunk {
        static_assert(std::is_trivially_constructible_v<T>);

        // Compute how many items T will fit in chunk_size_bytes memory
        static constexpr size_t max_size = (ChunkSize - sizeof(size_t)) / sizeof(T);
        static_assert(max_size > 0);

        T data[max_size];
        size_t currentSize { 0 };
    };
    static_assert(std::alignment_of_v<T> <= std::alignment_of_v<Chunk> && std::alignment_of_v<T> % 2 == 0); // Memory blocks are 64 byte aligned
    static_assert(sizeof(Chunk) <= ChunkSize);

    //  Prevent false sharing
    struct alignas(cacheline_size) ThreadData {
        Chunk* pPartialChunk { nullptr };
    };
    tbb::scalable_allocator<MemoryBlock<ChunkSize>>& m_allocator;
    std::array<ThreadData, max_thread_count> m_threadData;
    tbb::concurrent_queue<Chunk*> m_fullChunks;
};

template <typename T, size_t ChunkSize>
inline MPSCQueue<T, ChunkSize>::MPSCQueue(tbb::scalable_allocator<MemoryBlock<ChunkSize>>& allocator)
    : m_allocator(allocator)
{
}

template <typename T, size_t ChunkSize>
inline void MPSCQueue<T, ChunkSize>::push(const T& item)
{
    push(gsl::span(&item, 1));
}

template <typename T, size_t ChunkSize>
inline void MPSCQueue<T, ChunkSize>::push(gsl::span<const T> data)
{
    const int currentThreadIndex = tbb::this_task_arena::current_thread_index();
    Chunk*& pThreadLocalChunk = m_threadData[currentThreadIndex].pPartialChunk;

    size_t currentIndex = 0;
    while (currentIndex < static_cast<size_t>(data.size())) {
        if (pThreadLocalChunk == nullptr)
            pThreadLocalChunk = allocateChunk();

        const size_t currentChunkSize = pThreadLocalChunk->currentSize;
        const size_t itemsLeftToCopy = data.size() - currentIndex;
        const size_t itemsToCopy = std::min(itemsLeftToCopy, Chunk::max_size - currentChunkSize);
        std::copy(
            std::begin(data) + currentIndex,
            std::begin(data) + currentIndex + itemsToCopy,
            std::begin(pThreadLocalChunk->data) + currentChunkSize);
        currentIndex += itemsToCopy;

        pThreadLocalChunk->currentSize += itemsToCopy;
        if (pThreadLocalChunk->currentSize == Chunk::max_size)
            forwardThreadLocalChunk(currentThreadIndex);
    }
}

template <typename T, size_t ChunkSize>
inline void MPSCQueue<T, ChunkSize>::forwardThreadLocalChunksUnsafe()
{
    Chunk* pSharedChunk = nullptr;

    for (int i = 0; i < tbb::this_task_arena::max_concurrency(); i++) {
        Chunk*& pThreadLocalChunk = m_threadData[i].pPartialChunk;
        if (pThreadLocalChunk) {
            const size_t itemsToCopy = pThreadLocalChunk->data.size();

            size_t currentIndex = 0;
            while (currentIndex < pThreadLocalChunk->currentSize) {
                if (pSharedChunk == nullptr)
                    pSharedChunk = allocateChunk();

                const size_t currentChunkSize = pSharedChunk->currentSize;
                const size_t itemsLeftToCopy = pThreadLocalChunk->currentSize - currentIndex;
                const size_t itemsToCopy = std::min(itemsLeftToCopy, Chunk<T>::max_size - currentChunkSize);
                std::copy(
                    std::begin(pThreadLocalChunk->data) + currentIndex,
                    std::begin(pThreadLocalChunk->data) + currentIndex + itemsToCopy,
                    std::begin(pSharedChunk->data) + currentChunkSize);
                currentIndex += itemsToCopy;

                pSharedChunk->currentSize += itemsToCopy;
                if (pSharedChunk->currentSize == Chunk::max_size) {
                    fullChunkes.push(pSharedChunk);
                    pSharedChunk = nullptr;
                }
            }

            deallocateChunk(pThreadLocalChunk);
        }
        pThreadLocalChunk = nullptr;
    }

    if (pSharedChunk)
        fullChunkes.push(pSharedChunk);
}

template <typename T, size_t ChunkSize>
inline void MPSCQueue<T, ChunkSize>::forwardThreadLocalChunk(int threadIndex)
{
    Chunk*& pThreadLocalChunk = m_threadData[threadIndex].pPartialChunk;
    m_fullChunks.push(pThreadLocalChunk);
    pThreadLocalChunk = nullptr;
}

template <typename T, size_t ChunkSize>
inline typename MPSCQueue<T, ChunkSize>::Chunk* MPSCQueue<T, ChunkSize>::allocateChunk()
{
    MemoryBlock<ChunkSize>* pMemory = m_allocator.allocate(1);
    return new (pMemory) Chunk();
}

template <typename T, size_t ChunkSize>
inline void MPSCQueue<T, ChunkSize>::deallocateChunk(Chunk* pChunk)
{
    pChunk->~Chunk<T>();
    MemoryBlock* pMemory = reinterpret_cast<MemoryBlock*>(pChunk);
    m_chunkAllocator.deallocate(pMemory, 1);
}

}