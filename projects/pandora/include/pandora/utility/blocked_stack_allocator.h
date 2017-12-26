#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>

namespace pandora {

class BlockedStackAllocator {
public:
    static const size_t maxAlignment = 64;

    BlockedStackAllocator(size_t blockSizeBytes = 4096);

    template <class T>
    T* allocate(size_t n = 1, size_t alignment = alignof(T)); // Allocate multiple items at once (contiguous in memory)

    void reset();

private:
    // Global allocation pool
    size_t m_memoryBlockSize;
    tbb::concurrent_vector<std::unique_ptr<std::byte[]>> m_memoryBlocks;

    struct ThreadLocalData {
        ThreadLocalData()
            : data(nullptr)
            , space(0)
        {
        }
        std::byte* data;
        size_t space; // In bytes
    };
    tbb::enumerable_thread_specific<ThreadLocalData> m_threadLocalBlocks;
};

template <class T>
T* BlockedStackAllocator::allocate(size_t n, size_t alignment)
{
    // Assert that the requested alignment is a power of 2 (a requirement in C++ 17)
    // https://stackoverflow.com/questions/10585450/how-do-i-check-if-a-template-parameter-is-a-power-of-two
    assert((alignment & (alignment - 1)) == 0);

    auto& localBlock = m_threadLocalBlocks.local();

    // Amount of bytes to allocate
    size_t amount = n * sizeof(T);

    // It is never going to fit in the current block, so allocate a new block
    if (localBlock.space < amount) {
        // Allocation failed because the block is full. Allocate a new block and try again.
        auto newMemBlock = m_memoryBlocks.emplace_back(new std::byte[m_memoryBlockSize]);
        //m_memoryBlocks.emplace_back(m_memoryBlockSize);
        localBlock.data = newMemBlock->get();
        localBlock.space = m_memoryBlockSize;
    }

    // Try to get the next available aligned address in the current block of memory
    // http://en.cppreference.com/w/cpp/memory/align
    void* ptr = localBlock.data;
    size_t space = localBlock.space;
    if (void* alignedPtr = std::align(alignment, amount, ptr, space)) {
        T* result = reinterpret_cast<T*>(ptr);
        localBlock.data = (std::byte*)ptr + amount;
        localBlock.space -= amount;
        return result;
    }

    // Allocation failed because the block is full. Allocate a new block and try again.
    auto newMemBlock = m_memoryBlocks.emplace_back(new std::byte[m_memoryBlockSize]);
    localBlock.data = newMemBlock->get();
    localBlock.space = m_memoryBlockSize;

    // Try to get the next available aligned address in the current block of memory
    // http://en.cppreference.com/w/cpp/memory/align
    ptr = localBlock.data;
    space = localBlock.space;
    if (std::align(alignment, amount, ptr, space)) {
        T* result = reinterpret_cast<T*>(ptr);
        localBlock.data = (std::byte*)ptr + amount;
        localBlock.space -= amount;
        return result;
    }

    // If it failed again then we either ran out of memory or the amount we tried to allocate does not fit in our memory pool
    return nullptr;
}
}