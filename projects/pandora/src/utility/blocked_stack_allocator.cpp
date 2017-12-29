#include "pandora/utility/blocked_stack_allocator.h"

namespace pandora {
BlockedStackAllocator::BlockedStackAllocator(size_t blockSize)
    : m_memoryBlockSize(blockSize)
    , m_memoryBlocks()
    , m_threadLocalBlocks(ThreadLocalData())
{
}

void BlockedStackAllocator::reset()
{
    for (auto& localBlock : m_threadLocalBlocks) {
        localBlock.data = nullptr;
        localBlock.space = 0;
    }

    // Deallocate all memory associated with this allocator
    m_memoryBlocks.clear();
}

void BlockedStackAllocator::allocateBlock()
{
    auto& localBlock = m_threadLocalBlocks.local();

    // Allocation failed because the block is full. Allocate a new block and try again.
    auto newMemBlock = m_memoryBlocks.emplace_back(new std::byte[m_memoryBlockSize]);
    localBlock.data = newMemBlock->get();
    localBlock.space = m_memoryBlockSize;
}

void* BlockedStackAllocator::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
{
    auto& localBlock = m_threadLocalBlocks.local();

    // Try to get the next available aligned address in the current block of memory
    // http://en.cppreference.com/w/cpp/memory/align
    void* ptr = localBlock.data;
    size_t space = localBlock.space;
    std::byte* oldDataPtr = localBlock.data;
    if (void* alignedPtr = std::align(alignment, amount, ptr, space)) {
        std::byte* newDataPtr = (std::byte*)ptr + amount;
        localBlock.data = newDataPtr;
        localBlock.space -= newDataPtr - oldDataPtr;
        return alignedPtr;
    }

    return nullptr;
}
}