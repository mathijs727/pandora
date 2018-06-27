#include "pandora/utility/memory_arena.h"

namespace pandora {
MemoryArena::MemoryArena(size_t blockSize)
    : m_memoryBlockSize(blockSize)
    , m_usedMemoryBlocks()
    , m_unusedMemoryBlocks()
    , m_currentBlockData(nullptr)
    , m_currentBlockSpace(0)
{
}

void MemoryArena::reset()
{
    m_currentBlockData = nullptr;
    m_currentBlockSpace = 0;

    // Move all used memory blocks to the unused pool
    m_unusedMemoryBlocks.reserve(m_unusedMemoryBlocks.size () + m_usedMemoryBlocks.size());
    std::move(std::begin(m_usedMemoryBlocks), std::end(m_usedMemoryBlocks), std::back_inserter(m_unusedMemoryBlocks));
    m_usedMemoryBlocks.clear();
}

void MemoryArena::allocateBlock()
{
    std::unique_ptr<std::byte[]> data;
    if (m_unusedMemoryBlocks.empty())
    {
        data = std::make_unique<std::byte[]>(m_memoryBlockSize);
    } else {
        data = std::move(m_unusedMemoryBlocks.back());
        m_unusedMemoryBlocks.pop_back();
    }
    m_currentBlockData = data.get();
    m_currentBlockSpace = m_memoryBlockSize;
    m_usedMemoryBlocks.push_back(std::move(data));
}

void* MemoryArena::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
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
