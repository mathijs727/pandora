#pragma once
#include "pandora/utility/growing_free_list_ts.h"
#include <array>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <cassert>

namespace pandora {

template <size_t BlockSize>
class FreeListBackedMemoryArena {
public:
    static const size_t maxAlignment = 64;
    struct alignas(8) MemoryBlock {
        std::array<std::byte, BlockSize> data;
    };

    FreeListBackedMemoryArena(GrowingFreeListTS<MemoryBlock>& backingAllocator);
    ~FreeListBackedMemoryArena();

    template <typename T, typename... Args>
    T* allocate(Args... args);

    void reset();

private:
    void allocateBlock();
    void* tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment);

private:
    // Currently allocated blocks (that we should return to the freelist on reset)
    GrowingFreeListTS<MemoryBlock>& m_backingAllocator;
    std::vector<MemoryBlock*> m_usedMemoryBlocks;

    std::byte* m_currentBlockData;
    size_t m_currentBlockSpace; // In bytes
};

template <size_t BlockSize>
inline FreeListBackedMemoryArena<BlockSize>::FreeListBackedMemoryArena(GrowingFreeListTS<MemoryBlock>& backingAllocator)
    : m_backingAllocator(backingAllocator)
    , m_usedMemoryBlocks()
    , m_currentBlockData(nullptr)
    , m_currentBlockSpace(0)
{
}

template <size_t BlockSize>
inline FreeListBackedMemoryArena<BlockSize>::~FreeListBackedMemoryArena()
{
    reset();
}

template <size_t BlockSize>
template <typename T, typename... Args>
inline T* FreeListBackedMemoryArena<BlockSize>::allocate(Args... args)
{
    // Assert that the requested alignment is a power of 2 (a requirement in C++ 17)
    // https://stackoverflow.com/questions/10585450/how-do-i-check-if-a-template-parameter-is-a-power-of-two
    auto alignment = std::alignment_of_v<T>;
    assert((alignment & (alignment - 1)) == 0);

    // Amount of bytes to allocate
    size_t amount = sizeof(T);

    // If the amount we try to allocate is larger than the memory block then the allocation will always fail
    assert(amount <= BlockSize);

    // It is never going to fit in the current block, so allocate a new block
    if (m_currentBlockSpace < amount) {
        allocateBlock();
    }

    // Try to make an aligned allocation in the current block
    if (void* ptr = tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // Allocation failed because the block is full. Allocate a new block and try again
    allocateBlock();

    // Try again
    if (T* ptr = (T*)tryAlignedAllocInCurrentBlock(amount, alignment)) {
        return new (ptr) T(std::forward<Args>(args)...);
    }

    // If it failed again then we either ran out of memory or the amount we tried to allocate does not fit in our memory pool
    return nullptr;
}

template <size_t BlockSize>
inline void FreeListBackedMemoryArena<BlockSize>::reset()
{
    m_currentBlockData = nullptr;
    m_currentBlockSpace = 0;

    for (auto* block : m_usedMemoryBlocks) {
        m_backingAllocator.deallocate(block);
    }
    m_usedMemoryBlocks.clear();
}

template <size_t BlockSize>
inline void FreeListBackedMemoryArena<BlockSize>::allocateBlock()
{
    auto* block = m_backingAllocator.allocate();
    m_currentBlockData = block->data.data();
    m_currentBlockSpace = BlockSize;
    m_usedMemoryBlocks.push_back(block);
}

template <size_t BlockSize>
inline void* FreeListBackedMemoryArena<BlockSize>::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
{
    // Try to get the next available aligned address in the current block of memory
    // http://en.cppreference.com/w/cpp/memory/align
    void* ptr = m_currentBlockData;
    size_t space = m_currentBlockSpace;
    if (std::align(alignment, amount, ptr, space)) {
        void* result = ptr;
        m_currentBlockData = reinterpret_cast<std::byte*>(ptr) + amount;
        m_currentBlockSpace = space - amount;
        return result;
    }

    return nullptr;
}

}