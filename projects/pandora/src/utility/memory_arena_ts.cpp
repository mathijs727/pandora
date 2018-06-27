#include "pandora/utility/memory_arena_ts.h"

namespace pandora {
MemoryArenaTS::MemoryArenaTS(size_t blockSize)
    : m_memoryBlockSize(blockSize)
    , m_memoryBlocks()
    , m_threadLocalBlocks(ThreadLocalData())
{
}

void MemoryArenaTS::reset()
{
    for (auto& localBlock : m_threadLocalBlocks) {
        localBlock.data = nullptr;
        localBlock.space = 0;
    }

    // Deallocate all memory associated with this allocator
    m_memoryBlocks.clear();
}

void MemoryArenaTS::allocateBlock()
{
    auto& localBlock = m_threadLocalBlocks.local();

    auto newMemBlock = m_memoryBlocks.emplace_back(new std::byte[m_memoryBlockSize]);
    //auto newMemBlock = m_memoryBlocks.push_back(std::move(std::make_unique<std::byte[]>(m_memoryBlockSize)));
    localBlock.data = newMemBlock->get();
    localBlock.space = m_memoryBlockSize;
}

void* MemoryArenaTS::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
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
