#pragma once
#include <atomic>

namespace stream {

class EvictLRUCache;

template <typename T>
class CachedPtr {
public:
    CachedPtr(CachedPtr&& other) noexcept;
    CachedPtr(const CachedPtr& other) noexcept;
    ~CachedPtr();

    T& operator=(CachedPtr&& other) noexcept;
    T& operator=(const CachedPtr& other) noexcept;

    T& operator*() const noexcept;
    T* operator->() const noexcept;

    T* get() const noexcept;

private:
    friend class EvictLRUCache;
    CachedPtr(T* pItem, std::atomic_int* pRefCount);

private:
    T* m_pItem;
    std::atomic_int* m_pRefCount;
};

template <typename T>
inline CachedPtr<T>::CachedPtr(T* pItem, std::atomic_int* pRefCount)
    : m_pItem(pItem)
    , m_pRefCount(pRefCount)
{
    m_pRefCount->fetch_add(1);
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
    m_pRefCount->fetch_add(1);
}

template <typename T>
inline CachedPtr<T>::~CachedPtr()
{
    if (m_pRefCount)
        m_pRefCount->fetch_sub(1);
}
template <typename T>
inline T& CachedPtr<T>::operator=(CachedPtr&& other) noexcept
{
    m_pItem = other.m_pItem;
    m_pRefCount = other.m_pRefCount;
    other.m_pItem = nullptr;
    other.m_pRefCount = nullptr;
    return *this;
}
template <typename T>
inline T& CachedPtr<T>::operator=(const CachedPtr& other) noexcept
{
    m_pItem = other.m_pItem;
    m_pRefCount = other.m_pRefCount;
    m_pRefCount->fetch_add(1);
    return *this;
}

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