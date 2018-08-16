#pragma once
#include "pandora/utility/error_handling.h"
#include <atomic>
#include <cstddef>
#include <tbb/enumerable_thread_specific.h>
#include <vector>

namespace pandora {

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
    template <int Padding>
    struct alignas(std::alignment_of_v<T>) _FreeListNode {
        _FreeListNode<Padding>* next;
    private:
        std::byte __padding[Padding];
    };

    // Use template specialization to disable the padding bytes when the padding is 0
    template <>
    struct alignas(std::alignment_of_v<T>) _FreeListNode<0> {
        _FreeListNode<0>* next;
    };

    static constexpr size_t s_padding = sizeof(T) - sizeof(_FreeListNode<0>);
    using FreeListNode = _FreeListNode<s_padding>;

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
