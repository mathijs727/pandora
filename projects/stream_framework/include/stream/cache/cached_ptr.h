#pragma once
#include <atomic>

namespace tasking {

class EvictLRUCache;

template <typename T>
class CachedPtr {
public:
    CachedPtr() = default;
    CachedPtr(CachedPtr&& other) noexcept;
    CachedPtr(const CachedPtr& other) noexcept;
    ~CachedPtr();

    CachedPtr<T>& operator=(CachedPtr&& other) noexcept;
    //CachedPtr<T>& operator=(const CachedPtr& other) noexcept;

    T& operator*() const noexcept;
    T* operator->() const noexcept;

    T* get() const noexcept;

private:
    friend class LRUCache;
    friend class LRUCacheTS;
    friend class DummyCache;
    CachedPtr(T* pItem, std::atomic_int* pRefCount, bool shouldIncrement = true);

private:
    T* m_pItem { nullptr };
    std::atomic_int* m_pRefCount { nullptr };
};

template <typename T>
inline CachedPtr<T>::CachedPtr(T* pItem, std::atomic_int* pRefCount, bool shouldIncrement)
    : m_pItem(pItem)
    , m_pRefCount(pRefCount)
{
    if (shouldIncrement && m_pRefCount)
        m_pRefCount->fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
inline CachedPtr<T>::CachedPtr(CachedPtr&& other) noexcept
    : m_pItem(other.m_pItem)
    , m_pRefCount(other.m_pRefCount)
{
    other.m_pItem = nullptr;
    other.m_pRefCount = nullptr;
}

template <typename T>
inline CachedPtr<T>::CachedPtr(const CachedPtr& other) noexcept
    : m_pItem(other.m_pItem)
    , m_pRefCount(other.m_pRefCount)
{
    if (m_pRefCount)
        m_pRefCount->fetch_add(1, std::memory_order_relaxed);
}

template <typename T>
inline CachedPtr<T>::~CachedPtr()
{
    if (m_pRefCount)
        m_pRefCount->fetch_sub(1, std::memory_order_relaxed);
}

template <typename T>
inline CachedPtr<T>& CachedPtr<T>::operator=(CachedPtr&& other) noexcept
{
    m_pItem = other.m_pItem;
    m_pRefCount = other.m_pRefCount;
    other.m_pItem = nullptr;
    other.m_pRefCount = nullptr;
    return *this;
}

/*template <typename T>
inline CachedPtr<T>& CachedPtr<T>::operator=(const CachedPtr& other) noexcept
{
    if (m_pRefCount)
        m_pRefCount->fetch_sub(1, std::memory_order_relaxed);

    m_pItem = other.m_pItem;
    m_pRefCount = other.m_pRefCount;
    if (m_pRefCount)
        m_pRefCount->fetch_add(1, std::memory_order_relaxed);

    return *this;
}*/

template <typename T>
inline T& CachedPtr<T>::operator*() const noexcept
{
    return *m_pItem;
}
template <typename T>
inline T* CachedPtr<T>::operator->() const noexcept
{
    return m_pItem;
}

template <typename T>
inline T* CachedPtr<T>::get() const noexcept
{
    return m_pItem;
}
}