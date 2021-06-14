#pragma once
#include "stream/cache/cache.h"
#include "stream/cache/cached_ptr.h"
#include "stream/cache/evictable.h"
#include "stream/cache/handle.h"
#include "stream/serialize/serializer.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <span>
#include <span>
#include <list>
#include <mutex>
#include <optick.h>
#include <unordered_map>
#include <vector>

namespace tasking {

class LRUCacheTS {
public:
    class Builder;

    ~LRUCacheTS();

    LRUCacheTS(LRUCacheTS&&) noexcept;
    LRUCacheTS& operator=(LRUCacheTS&&) noexcept;

    template <typename T>
    CachedPtr<T> makeResident(T* pEvictable);

    void forceEvict(Evictable* pEvictable);

    size_t memoryUsage() const noexcept;
    size_t maxSize() const;

private:
    LRUCacheTS(std::unique_ptr<tasking::Deserializer>&& pDeserializer, std::span<Evictable*> items, size_t maxMemory);

    void evictMarked();

private:
    std::unique_ptr<tasking::Deserializer> m_pDeserializer;

    size_t m_maxMemory;
    std::atomic_size_t m_usedMemory { 0 };

    enum class ItemState : uint32_t {
        Unloaded,
        Loading,
        Loaded,
        Evicting
    };
    struct ItemData {
        std::atomic_bool marked { true }; // Recently accessed?
        std::atomic<ItemState> state { ItemState::Unloaded };
        std::atomic_int refCount { 0 };
    };
    std::unique_ptr<ItemData[]> m_pItemData;
    std::unordered_map<Evictable*, uint32_t> m_itemDataIndices;
    std::mutex m_evictMutex;
};

class LRUCacheTS::Builder : public CacheBuilder {
public:
    Builder(std::unique_ptr<tasking::Serializer>&& pSerializer);

    void registerCacheable(Evictable* pItem, bool evict = false) override;

    LRUCacheTS build(size_t maxMemory);

private:
    std::unique_ptr<tasking::Serializer> m_pSerializer;
    std::vector<Evictable*> m_items;
};

template <typename T>
inline CachedPtr<T> LRUCacheTS::makeResident(T* pEvictable)
{
    auto& itemData = m_pItemData[m_itemDataIndices[pEvictable]];
    if (itemData.marked.load(std::memory_order_relaxed))
        itemData.marked.store(false);

    // Ensure that the item will not be deleted by immediately increasing the reference count.
    itemData.refCount.fetch_add(1, std::memory_order_relaxed);

    // In case the state was changed to evicting then we need to wait for the other thread
    // to finish evicting. This can only happen once: the evict function is a critical section
    // so any subsequent call to evict will see that the ref count has been increased.
    ItemState state;
    do {
        state = itemData.state.load(std::memory_order_acquire);
    } while (state == ItemState::Evicting);

    // We have increased the reference count so no thread may evict the data. However we still
    // need to check whether it was loaded (or is being loaded) in a thread safe manner.
    if (state == ItemState::Loaded) {
        return CachedPtr<T>(pEvictable, &itemData.refCount, false);
    }

    if (state == ItemState::Unloaded) {
        if (itemData.state.compare_exchange_strong(state, ItemState::Loading, std::memory_order_acquire)) {
            const size_t sizeBefore = pEvictable->sizeBytes();
            pEvictable->makeResident(*m_pDeserializer);
            const size_t sizeAfter = pEvictable->sizeBytes();
            assert(sizeAfter >= sizeBefore);
            m_usedMemory.fetch_add(sizeAfter - sizeBefore, std::memory_order_relaxed);

            itemData.state.store(ItemState::Loaded, std::memory_order_release);

            if (m_usedMemory > m_maxMemory)
                evictMarked();
        } else {
            // Other thread already started loading, we need to wait...
            while (itemData.state.load(std::memory_order_acquire) != ItemState::Loaded)
                continue;
        }
    } else {
        // Other thread already started loading, we need to wait...
        while (itemData.state.load(std::memory_order_acquire) != ItemState::Loaded)
            continue;
    }

    return CachedPtr<T>(pEvictable, &itemData.refCount, false);
}

}