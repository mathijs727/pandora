#pragma once
#include "traits.h"
#include <atomic>
#include <memory>
#include <tuple>
#include <optional>

namespace tasking {

// Type should have SubResource
template <typename... T>
class ResourceCache {
public:
    ResourceCache(size_t expectedSize);

    template <typename S, typename = std::enable_if_t<is_contained<S, T...>::value>>
    class RefCountedHandle {
    public:
        ~RefCountedHandle()
        {
            m_refCount--;
        }

        S* operator->()
        {
            return m_value;
        }
        const S* operator->() const
        {
            return m_value;
        }

    private:
        RefCountedHandle();

    private:
        S* m_value;
        std::atomic_int* m_refCount;
    };

    using ResourceID = uint32_t;

    template <typename S, typename = std::enable_if_t<is_contained<S, T...>::value>>
    std::optional<RefCountedHandle<S>> acquireOrConstructResource()
    {

    }

    template <typename S, typename = std::enable_if_t<is_contained<S, T...>::value>>
    RefCountedHandle<S> tryAcquireResource()
    {
    }

private:
    std::tuple<tbb::scalable_allocator<std::pair<std::atomic_int, T...>>> m_allocators;
    tbb::concurrent_hash_map<ResourceID, void*> m_hashMap;
};
}
