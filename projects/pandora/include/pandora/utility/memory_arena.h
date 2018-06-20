#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

namespace pandora {

class MemoryArena {
public:
    static const size_t maxAlignment = 64;

    MemoryArena(size_t blockSizeBytes = 4096);

    template <class T, class... Args>
    T* allocate(Args... args); // Allocate multiple items at once (contiguous in memory)

    template <class T, int alignment, class... Args>
    T* allocate(Args... args); // Allocate multiple items at once (contiguous in memory)

    void reset();

private:
    void allocateBlock();
    void* tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment);

private:
    // Global allocation pool
    const size_t m_memoryBlockSize;
    std::vector<std::unique_ptr<std::byte[]>> m_memoryBlocks;

    std::byte* m_currentBlockData;
    size_t m_currentBlockSpace; // In bytes
};

template <class T, class... Args>
inline T* MemoryArena::allocate(Args... args)
{
    return allocate<T, std::alignment_of<T>::value, Args...>(args...);
}

template <class T, int alignment, class... Args>
inline T* MemoryArena::allocate(Args... args)
{
    // Assert that the requested alignment is a power of 2 (a requirement in C++ 17)
    // https://stackoverflow.com/questions/10585450/how-do-i-check-if-a-template-parameter-is-a-power-of-two
    assert((alignment & (alignment - 1)) == 0);

    // Amount of bytes to allocate
    size_t amount = sizeof(T);

    // If the amount we try to allocate is larger than the memory block then the allocation will always fail
    assert(amount <= m_memoryBlockSize);

    // It is never going to fit in the current block, so allocate a new block
    if (m_currentBlockSpace < amount) {
        allocateBlock();
    }

    // Try to make an aligned allocation in the current block
    if (void* ptr = tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return new (ptr) T(args...);
    }

    // Allocation failed because the block is full. Allocate a new block and try again
    allocateBlock();

    // Try again
    if (T* ptr = (T*)tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return new (ptr) T(args...);
    }

    // If it failed again then we either ran out of memory or the amount we tried to allocate does not fit in our memory pool
    return nullptr;
}
}
