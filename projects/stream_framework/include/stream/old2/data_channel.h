#pragma once
#include <EASTL/fixed_vector.h>
#include <array>
#include <cstddef>
#include <span>
#include <optional>
#include <tbb/scalable_allocator.h>
#include <type_traits>

namespace tasking {

constexpr size_t MemoryBlockSize = 4096;
using MemoryBlock = std::array<unsigned char, MemoryBlockSize>;
using MemoryBlockAllocator = tbb::scalable_allocator<MemoryBlock>;
//using MemoryBlockAllocator = FreeListAllocator<MemoryBlock>;

template <typename T>
class DataBlock {
public:
    static_assert(std::is_trivially_constructible_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_copy_assignable_v<T>);
	// NOTE: user (producer/consumer) is responsible for allocation / deallocation of underlying memory
    DataBlock() = default; // Required for (tbb) bounded queue
    DataBlock(MemoryBlock* pMemoryBlock);

    void push(const T& t);
    std::span<T> data();
    std::span<const T> data() const;

    bool empty() const;
    bool full() const;

public:
    MemoryBlock* pMemoryBlock { nullptr };

private:
    T* m_pFirst { nullptr };
    T* m_pLast { nullptr };
    T* m_pCurrent { nullptr };
};

template <typename T>
class DataChannel {
public:
    virtual void pushBlocking(DataBlock<T>&& dataBlock) = 0;
    virtual std::optional<DataBlock<T>> tryPop() = 0;
};

template <typename T>
inline DataBlock<T>::DataBlock(MemoryBlock* pMemoryBlock)
    : pMemoryBlock(pMemoryBlock)
    , m_pFirst(reinterpret_cast<T*>(pMemoryBlock))
    , m_pLast(m_pFirst + MemoryBlockSize / sizeof(T))
    , m_pCurrent(m_pFirst)
{
}

template <typename T>
inline void DataBlock<T>::push(const T& t)
{
    *(m_pCurrent++) = t;
}

template <typename T>
inline std::span<T> DataBlock<T>::data()
{
    return std::span(m_pFirst, m_pCurrent);
}

template <typename T>
inline std::span<const T> DataBlock<T>::data() const
{
    return std::span(m_pFirst, m_pCurrent);
}

template <typename T>
inline bool DataBlock<T>::empty() const
{
    return m_pCurrent == m_pFirst;
}

template <typename T>
inline bool DataBlock<T>::full() const
{
    return m_pCurrent == m_pLast;
}

}
