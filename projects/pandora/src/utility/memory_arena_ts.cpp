#include "pandora/utility/memory_arena_ts.h"

namespace pandora {
MemoryArenaTS::MemoryArenaTS(uint32_t blockSize)
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
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& localBlock = m_threadLocalBlocks.local();
	
    m_memoryBlocks.push_back(std::move(std::make_unique<std::byte[]>(m_memoryBlockSize)));
    auto& newMemBlock = m_memoryBlocks.back();
    localBlock.blockIndex = (uint32_t)m_memoryBlocks.size() - 1;
	localBlock.data = newMemBlock.get();
	localBlock.start = newMemBlock.get();
    localBlock.space = m_memoryBlockSize;
}

void* MemoryArenaTS::tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment)
{
    auto& localBlock = m_threadLocalBlocks.local();

	// Try to get the next available aligned address in the current block of memory
	// http://en.cppreference.com/w/cpp/memory/align
	void* ptr = localBlock.data;
	size_t space = localBlock.space;
	if (std::align(alignment, amount, ptr, space)) {
		void* result = ptr;
		localBlock.data = reinterpret_cast<std::byte*>(ptr) + amount;
		localBlock.space = static_cast<uint32_t>(space - amount);
		return result;
	}

	return nullptr;
}
}
