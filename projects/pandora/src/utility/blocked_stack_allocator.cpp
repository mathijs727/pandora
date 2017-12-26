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
}
