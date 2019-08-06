#pragma once
#include <gsl/span>
#include <memory_resource>
#include <type_traits>

namespace tasking {

template <typename T>
inline gsl::span<T> allocateN(std::pmr::memory_resource* pMemoryResource, size_t N)
{
    static_assert(std::is_trivially_constructible_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);

    void* pMemory = pMemoryResource->allocate(N * sizeof(T), std::alignment_of_v<T>);
    T* pFirst = reinterpret_cast<T*>(pMemory);
    T* pLast = pFirst + N;
    return gsl::make_span(pFirst, pLast);
}

template <typename T, typename... Args>
inline T* allocate(std::pmr::memory_resource* pMemoryResource, Args&&... args)
{
    static_assert(std::is_trivially_constructible_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);

    void* pMemory = pMemoryResource->allocate(N * sizeof(T), std::alignment_of_v<T>);
    return new (pMemory) T(std::forward<Args>(args...));
}

}