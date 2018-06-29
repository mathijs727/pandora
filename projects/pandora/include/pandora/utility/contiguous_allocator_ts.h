#pragma once
#include "pandora/utility/error_handling.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>

namespace pandora {

template <typename T>
class ContiguousAllocatorTS {
public:
    ContiguousAllocatorTS(uint32_t maxSize, uint32_t blockSize = std::min(4096, maxSize));

    using Handle = uint32_t;
	template <typename... Args>
	std::pair<Handle, T*> allocate(Args... args);

	template <int N, typename... Args>
	std::pair<Handle, T*> allocateN(Args... args);

    inline T& get(Handle handle) const { return m_start[handle]; };

private:
	uint32_t allocateBlock();

private:
    const uint32_t m_maxSize;
    const uint32_t m_blockSize;

    std::unique_ptr<T[]> m_start;
    std::atomic_uint32_t m_currentSize;

    struct ThreadLocalData {
		uint32_t index = 0;
		uint32_t space = 0;
    };
    tbb::enumerable_thread_specific<ThreadLocalData> m_threadLocalBlocks;
};

template <typename T>
inline ContiguousAllocatorTS<T>::ContiguousAllocatorTS(uint32_t maxSize, uint32_t blockSize)
    : m_maxSize(maxSize)
    , m_blockSize(blockSize)
    , m_start(new T[maxSize])
    , m_currentSize(0)
{
    assert(maxSize % blockSize == 0);
}

template<typename T>
template<typename... Args>
inline std::pair<typename ContiguousAllocatorTS<T>::Handle, T*> ContiguousAllocatorTS<T>::allocate(Args ...args)
{
	return allocateN<1>(args...);
}

template<typename T>
template<int N, typename ...Args>
inline std::pair<typename ContiguousAllocatorTS<T>::Handle, T*> ContiguousAllocatorTS<T>::allocateN(Args ...args)
{
	assert(N <= m_blockSize);

	auto& currentBlock = m_threadLocalBlocks.local();
	if (currentBlock.space < N) {
		currentBlock.index = allocateBlock();
		currentBlock.space = m_blockSize;
	}

	uint32_t resultIndex = currentBlock.index;
	currentBlock.index += N;
	currentBlock.space -= N;

	T* dataPtr = m_start.get() + resultIndex;
	for (uint32_t i = 0; i < N; i++)
		new (dataPtr + i) T(args...);

	return { resultIndex, dataPtr };
}

template <typename T>
inline uint32_t ContiguousAllocatorTS<T>::allocateBlock()
{
	uint32_t index = m_currentSize.fetch_add(m_blockSize);
	if (index + m_blockSize > m_maxSize)
		THROW_ERROR("ContiguousAllocator ran out of memory!");
	return index;
}

}
