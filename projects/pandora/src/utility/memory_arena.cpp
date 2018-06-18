#include "pandora/utility/memory_arena.h"

namespace pandora {
MemoryArena::MemoryArena(size_t blockSize)
    : m_memoryBlockSize(blockSize)
    , m_memoryBlocks()
    , m_currentBlockData(nullptr)
    , m_currentBlockSpace(0)
{
}

void MemoryArena::reset()
{
    m_currentBlockData = nullptr;
    m_currentBlockSpace = 0;

    // Deallocate all memory associated with this allocator
    m_memoryBlocks.clear();
}

void MemoryArena::allocateBlock()
{
    auto data = std::make_unique<std::byte[]>(m_memoryBlockSize);
    m_currentBlockData = data.get();
    m_currentBlockSpace = m_memoryBlockSize;
    m_memoryBlocks.push_back(std::move(data));
}

void* MemoryArena::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
{
    // Try to get the next available aligned address in the current block of memory
    // http://en.cppreference.com/w/cpp/memory/align
    void* ptr = m_currentBlockData;
    size_t space = m_currentBlockSpace;
    std::byte* oldDataPtr = m_currentBlockData;
    if (void* alignedPtr = std::align(alignment, amount, ptr, space)) {
        std::byte* newDataPtr = (std::byte*)ptr + amount;
        m_currentBlockData = newDataPtr;
        m_currentBlockSpace -= newDataPtr - oldDataPtr;
        return alignedPtr;
    }

    return nullptr;
}
}
