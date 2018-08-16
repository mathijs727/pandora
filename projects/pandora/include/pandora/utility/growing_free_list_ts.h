#pragma once
#include "pandora/utility/error_handling.h"
#include <atomic>
#include <cstddef>
#include <tbb/enumerable_thread_specific.h>
#include <vector>
#include <type_traits>

namespace pandora {

// Explicit specialization in non-namespace scope is not allowed in ISO C++
namespace growing_free_list_ts_impl
{

template <typename T, typename Enable = void>
struct _FreeListNode;

template <typename T>
struct alignas(std::alignment_of_v<T>) _FreeListNode<T, typename std::enable_if<(sizeof(T) > sizeof(void*))>::type>
{
    _FreeListNode<T>* next;
private:
    constexpr static size_t paddingAmount = sizeof(T) - sizeof(void*);
    std::byte __padding[paddingAmount];
};

template <typename T>
struct alignas(std::alignment_of_v<T>) _FreeListNode<T, typename std::enable_if<(sizeof(T) == sizeof(void*))>::type>
{
    _FreeListNode<T>* next;
};

}

// Thread-safe free list that does a heap allocation for each new item when it is full.
// WARNING: the freelist does not call T's destructors when it is deleted
template <typename T>
class GrowingFreeListTS {
public:
    static_assert(sizeof(T) >= sizeof(void*)); // Needs to be bigger because can't properly disable padding?

    GrowingFreeListTS();
    ~GrowingFreeListTS() = default;

    template <typename... Args>
    T* allocate(Args... args);

    void deallocate(T* p);

private:
    using FreeListNode = growing_free_list_ts_impl::_FreeListNode<T>;
    static_assert(sizeof(FreeListNode) == sizeof(T));

    std::atomic<FreeListNode*> m_freeListHead;

    tbb::enumerable_thread_specific<std::vector<std::unique_ptr<FreeListNode>>> m_allocatedNodes;
};

template <typename T>
inline GrowingFreeListTS<T>::GrowingFreeListTS()
    : m_freeListHead(nullptr)
{
}

template <typename T>
inline void GrowingFreeListTS<T>::deallocate(T* p)
{
    // https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange
    auto* newNode = reinterpret_cast<FreeListNode*>(p);

    // Put the current value of m_freeListHead into newNode->next
    newNode->next = m_freeListHead.load(std::memory_order_relaxed);

    // Now make newNode the new head, but if the head is no longer what's stored in newNode->next
    // (some other thread must have inserted a node just now) then put that new head into
    // newNode->next and try again.
    while (!std::atomic_compare_exchange_weak_explicit(
        &m_freeListHead,
        &newNode->next,
        newNode,
        std::memory_order_release,
        std::memory_order_relaxed)) {
        // Empty loop body
    }
}

template <typename T>
template <typename... Args>
inline T* GrowingFreeListTS<T>::allocate(Args... args)
{
    // Pop
    auto* node = m_freeListHead.load(std::memory_order_relaxed);
    while (node) {
        std::atomic_compare_exchange_weak_explicit(
            &m_freeListHead,
            &node,
            node->next,
            std::memory_order_relaxed,
            std::memory_order_relaxed); // For allocation we don't care whether writes/reads are reordered.
    }

    if (!node) {
        // We could not reuse an old unused node so we have to allocate a new one
        auto newNode = std::make_unique<FreeListNode>();
        node = newNode.get();
        m_allocatedNodes.local().push_back(std::move(newNode));
    }

    return new (reinterpret_cast<void*>(node)) T(args...);
}

}
