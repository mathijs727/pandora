#pragma once
#include <cstddef>
#include <deque>
#include <memory>
#include <type_traits>

namespace tasking {

template <typename T>
class FreeListAllocator {
public:
    static_assert(std::is_trivially_constructible_v<T>);
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_trivially_copy_assignable_v<T>);
    FreeListAllocator(size_t initialSize, bool mayGrow);

    T* allocate();
    void deallocate(T* item);

private:
    struct Header;
    static_assert(sizeof(T) > sizeof(Header*));
    struct Header {
        Header* pNext;
        std::byte __padding[sizeof(T) - sizeof(Header*)];
    };
    static_assert(sizeof(Header) == sizeof(T));
    Header* m_pHead;

    bool m_mayGrow;
    std::deque<T> m_data;
};

template <typename T>
inline FreeListAllocator<T>::FreeListAllocator(size_t initialSize, bool mayGrow)
    : m_mayGrow(mayGrow)
{
    m_data.resize(initialSize);

    Header* pHeaders = initialSize > 0 ? reinterpret_cast<Header*>(&m_data[0]) : nullptr;
    if (initialSize > 0) {
        reinterpret_cast<Header*>(&m_data[initialSize - 1])->pNext = nullptr;
        for (size_t i = 0; i < initialSize - 1; i++) {
            reinterpret_cast<Header*>(&m_data[i])->pNext = reinterpret_cast<Header*>(&m_data[i + 1]);
        }
    }

    m_pHead = pHeaders;
}

template <typename T>
inline T* FreeListAllocator<T>::allocate()
{
    if (m_pHead != nullptr) {
        T* pItem = reinterpret_cast<T*>(m_pHead);
        m_pHead = m_pHead->pNext;
        return pItem;
    } else if (m_mayGrow) {
        T& item = m_data.emplace_back();
        return &item;
    } else {
        throw std::exception();
    }
}

template <typename T>
inline void FreeListAllocator<T>::deallocate(T* item)
{
    Header* pHeader = reinterpret_cast<Header*>(item);
    pHeader->pNext = m_pHead;
    m_pHead = pHeader;
}
}
