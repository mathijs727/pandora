#pragma once
#include "pandora/utility/error_handling.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <tbb/enumerable_thread_specific.h>
#include <tuple>
#include <vector>

#ifndef NDEBUG
#define MEMORY_ARENA_TS_BOUNDS_CHECK 1
#else
#define MEMORY_ARENA_TS_BOUNDS_CHECK 0
#endif
namespace pandora {

class MemoryArenaTS {
public:
    template <typename T>
    struct HandleN {
    public:
		HandleN() = default;
        HandleN(uint32_t block, uint32_t offsetInBytes, uint8_t N);

		// These functions may be called in parallel with allocate calls
        T& get(MemoryArenaTS& arena);
        const T& get(const MemoryArenaTS& arena);

		// Faster than regular get BUT no thread synchronization (don't call allocate at the same time)
		T& getUnsafe(MemoryArenaTS& arena);
        const T& getUnsafe(const MemoryArenaTS& arena);

		HandleN<T>& operator++();
		HandleN<T> operator++(int);
    private:
        uint32_t byteInBlock;
#if MEMORY_ARENA_TS_BOUNDS_CHECK == 1
        uint16_t block;
        uint8_t i;
        uint8_t N;
#else
        uint32_t block;
#endif
    };

    template <typename T>
    using Handle = HandleN<T>;

public:
    static const size_t maxAlignment = 64;

    MemoryArenaTS(uint32_t blockSizeBytes = 4096);

    template <class T, class... Args>
    std::pair<Handle<T>, T*> allocate(Args... args);

    template <class T, uint8_t N, class... Args>
    std::pair<HandleN<T>, T*> allocateN(Args... args); // Allocate multiple items at once (contiguous in memory)

    void reset();

private:
    void allocateBlock();
    void* tryAlignedAllocInCurrentBlock(size_t amount, size_t alignment);

private:
	template <typename T>
	struct MemoryAlignment
	{
		static constexpr size_t alignment = std::alignment_of_v<T>;
		static constexpr size_t sizeWithAlignment = sizeof(T) + (alignment - 1 - (sizeof(T) - 1) % alignment);
	};

    // Global allocation pool
    const uint32_t m_memoryBlockSize;

    std::mutex m_mutex;
    std::vector<std::unique_ptr<std::byte[]>> m_memoryBlocks;

    struct ThreadLocalData {
        ThreadLocalData()
            : data(nullptr)
			, start(nullptr)
            , blockIndex(0)
            , space(0)
        {
        }
		std::byte* data;
		std::byte* start;
        uint32_t blockIndex;
        uint32_t space; // In bytes
    };
    tbb::enumerable_thread_specific<ThreadLocalData> m_threadLocalBlocks;
};

template <typename T>
inline MemoryArenaTS::HandleN<T>::HandleN(uint32_t block, uint32_t offsetInBytes, uint8_t N)
    : byteInBlock(offsetInBytes)
#if MEMORY_ARENA_TS_BOUNDS_CHECK == 1
    , block(static_cast<uint16_t>(block))
    , i(0)
    , N(N)
#else
    , block(block)
#endif
{
}

template <typename T>
inline T& MemoryArenaTS::HandleN<T>::get(MemoryArenaTS& arena)
{
    std::lock_guard<std::mutex> lock(arena.m_mutex);
    return reinterpret_cast<T&>(arena.m_memoryBlocks[block][byteInBlock]);
}

template <typename T>
inline const T& MemoryArenaTS::HandleN<T>::get(const MemoryArenaTS& arena)
{
    std::lock_guard<std::mutex> lock(arena.m_mutex);
    return reinterpret_cast<const T&>(arena.m_memoryBlocks[block][byteInBlock]);
}

template <typename T>
inline T& MemoryArenaTS::HandleN<T>::getUnsafe(MemoryArenaTS& arena)
{
    return reinterpret_cast<T&>(arena.m_memoryBlocks[block][byteInBlock]);
}

template <typename T>
inline const T& MemoryArenaTS::HandleN<T>::getUnsafe(const MemoryArenaTS& arena)
{
    return reinterpret_cast<const T&>(arena.m_memoryBlocks[block][byteInBlock]);
}

template <class T>
inline MemoryArenaTS::Handle<T>& MemoryArenaTS::HandleN<T>::operator++()
{
	byteInBlock += MemoryAlignment<T>::sizeWithAlignment;
#if MEMORY_ARENA_TS_BOUNDS_CHECK == 1
	i++;
	assert(i < N);
#endif
	return *this;
}

template <class T>
inline MemoryArenaTS::Handle<T> MemoryArenaTS::HandleN<T>::operator++(int)
{
	Handle<T> result = *this;
	byteInBlock += MemoryAlignment<T>::sizeWithAlignment;
#if MEMORY_ARENA_TS_BOUNDS_CHECK == 1
	i++;
	assert(i < N);
#endif
	return result;
}

template <class T, class... Args>
inline std::pair<MemoryArenaTS::Handle<T>, T*> MemoryArenaTS::allocate(Args... args)
{
    return allocateN<T, 1, Args...>(args...);
}

template <class T, uint8_t N, class... Args>
inline std::pair<MemoryArenaTS::HandleN<T>, T*> MemoryArenaTS::allocateN(Args... args)
{
    // Amount of bytes to allocate
	constexpr size_t alignment = MemoryAlignment<T>::alignment;
	constexpr size_t sizeWithAlignment = MemoryAlignment<T>::sizeWithAlignment;
	constexpr size_t amount = N * sizeWithAlignment;
    static_assert(sizeWithAlignment >= sizeof(T));
    static_assert(amount % alignment == 0);

    // If the amount we try to allocate is larger than the memory block then the allocation will always fail
    assert(amount <= m_memoryBlockSize);

    // If it is not going to fit in the current block then allocate a new one
    if (m_threadLocalBlocks.local().space < amount) {
        allocateBlock();
    }

    {
        // Try to make an aligned allocation in the current block
        auto localBlock = m_threadLocalBlocks.local();
        if (void* ptr = tryAlignedAllocInCurrentBlock(amount, alignment)) {
			size_t offsetInBytes = reinterpret_cast<std::byte*>(ptr) - localBlock.start;

            // Run constructors
            T* tPtr = reinterpret_cast<T*>(ptr);
            for (uint8_t i = 0; i < N; i++) {
                new (tPtr + i) T(args...);
            }

            auto result = HandleN<T>(localBlock.blockIndex, (uint32_t)offsetInBytes, N);
			assert(&result.get(*this) == ptr);
			return { result, tPtr };
        }
    }

    // Allocation failed because the block did not have enough space (taking into account alignment). Allocate a new block and try again
    allocateBlock();

	{
		// Try again
		auto localBlock = m_threadLocalBlocks.local();
		if (void* ptr = tryAlignedAllocInCurrentBlock(amount, alignment)) {
			size_t offsetInBytes = reinterpret_cast<std::byte*>(ptr) - localBlock.start;

			// Run constructors
			T* tPtr = reinterpret_cast<T*>(ptr);
			for (uint8_t i = 0; i < N; i++) {
				new (tPtr + i) T(args...);
			}

			auto result = HandleN<T>(localBlock.blockIndex, (uint32_t)offsetInBytes, N);
			assert(&result.get(*this) == ptr);
			return { result, tPtr };
		}


		// If it failed again then we either ran out of memory or the amount we tried to allocate does not fit in our memory pool
		//return nullptr;
		THROW_ERROR("Could not allocate more memory!");
		return { HandleN <T>(), nullptr };
	}
}

/*template <class T>
T* MemoryArenaTS::allocate(size_t alignment)
{
    // Assert that the requested alignment is a power of 2 (a requirement in C++ 17)
    // https://stackoverflow.com/questions/10585450/how-do-i-check-if-a-template-parameter-is-a-power-of-two
    assert((alignment & (alignment - 1)) == 0);

    // Amount of bytes to allocate
    size_t amount = sizeof(T);

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
}*/
}
