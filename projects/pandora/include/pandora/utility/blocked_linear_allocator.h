#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <tbb/concurrent_vector.h>
#include <tbb/enumerable_thread_specific.h>

namespace pandora {

class BlockedLinearAllocator {
public:
    static const size_t maxAlignment = 64;

    BlockedLinearAllocator(size_t blockSizeBytes = 4096);

    template <class T>
    T* allocate(size_t n = 1, size_t alignment = alignof(T)); // Allocate multiple items at once (contiguous in memory)

    void reset();

private:
    void allocateBlock();
    void* tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment);

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
T* BlockedLinearAllocator::allocate(size_t n, size_t alignment)
{
    // Assert that the requested alignment is a power of 2 (a requirement in C++ 17)
    // https://stackoverflow.com/questions/10585450/how-do-i-check-if-a-template-parameter-is-a-power-of-two
    assert((alignment & (alignment - 1)) == 0);

    // Amount of bytes to allocate
    size_t amount = n * sizeof(T);

    // If the amount we try to allocate is larger than the memory block then the allocation will always fail
    assert(amount <= m_memoryBlockSize);

    // It is never going to fit in the current block, so allocate a new block
    if (m_threadLocalBlocks.local().space < amount) {
        allocateBlock();
    }

    // Try to make an aligned allocation in the current block
    if (T* result = (T*)tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return result;
    }

    // Allocation failed because the block is full. Allocate a new block and try again
    allocateBlock();

    // Try again
    if (T* result = (T*)tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return result;
    }

    // If it failed again then we either ran out of memory or the amount we tried to allocate does not fit in our memory pool
    return nullptr;
}
}
