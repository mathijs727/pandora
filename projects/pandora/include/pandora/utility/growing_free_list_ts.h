#pragma once
#include "pandora/utility/error_handling.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <mutex>
#include <tbb/enumerable_thread_specific.h>
#include <type_traits>
#include <vector>
#include <tbb/concurrent_queue.h>
#include <memory>

namespace pandora {

// Explicit specialization in non-namespace scope is not allowed in ISO C++
namespace growing_free_list_ts_impl {

    template <typename T, typename Enable = void>
    struct _FreeListNode;

    template <typename T>
    struct alignas(std::alignment_of_v<T> < 8 ? 8 : std::alignment_of_v<T>) _FreeListNode<T, typename std::enable_if<(sizeof(T) > sizeof(void*))>::type> {
        _FreeListNode<T>* next;

    private:
        constexpr static size_t paddingAmount = sizeof(T) - sizeof(void*);
        std::byte __padding[paddingAmount];
    };

    template <typename T>
    struct alignas(std::alignment_of_v<T> < 8 ? 8 : std::alignment_of_v<T>) _FreeListNode<T, typename std::enable_if<(sizeof(T) == sizeof(void*))>::type> {
        _FreeListNode<T>* next;
    };

}

// Thread-safe free list that does a heap allocation for each new item when it is full.
// WARNING: the freelist does not call T's destructors when it is deleted
template <typename T>
class GrowingFreeListTS {
public:
    static_assert(sizeof(T) >= sizeof(void*)); // Needs to be bigger because can't properly disable padding?

    GrowingFreeListTS() = default;
    ~GrowingFreeListTS() = default;

    template <typename... Args>
    T* allocate(Args... args);

    void deallocate(T* p);

private:
    using FreeListNode = growing_free_list_ts_impl::_FreeListNode<T>;
    static_assert(sizeof(FreeListNode) == sizeof(T));
    //std::atomic<FreeListNode*> m_freeListHead;
    tbb::concurrent_queue<FreeListNode*> m_freeList;

    tbb::enumerable_thread_specific<std::vector<std::unique_ptr<FreeListNode>>> m_allocatedNodes;
};

template <typename T>
inline void GrowingFreeListTS<T>::deallocate(T* p)
{
    p->~T();

    // https://en.cppreference.com/w/cpp/atomic/atomic_compare_exchange
    auto* newNode = reinterpret_cast<FreeListNode*>(p);

    m_freeList.push(newNode);
}

template <typename T>
template <typename... Args>
inline T* GrowingFreeListTS<T>::allocate(Args... args)
{
    FreeListNode* node;
    if (!m_freeList.try_pop(node)) {
        // We could not reuse an old unused node so we have to allocate a new one
        auto newNode = std::make_unique<FreeListNode>();
        node = newNode.get();
        m_allocatedNodes.local().push_back(std::move(newNode));
    }

    return new (reinterpret_cast<void*>(node)) T(args...);
}

}
