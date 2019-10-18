#pragma once
#include "stream/cached_ptr.h"
#include "stream/evictable.h"
#include "stream/handle.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <gsl/span>
#include <unordered_map>
#include <vector>

namespace stream {

class EvictLRUCache {
public:
    class Builder;
    template <typename T>
    CachedPtr<T> get(CacheHandle<T> handle);

    size_t memoryUsage() const noexcept;
    bool checkResidencyIsValid() const;

private:
    EvictLRUCache(gsl::span<Evictable*> items, size_t maxMemory);

    void evict(size_t desiredMemoryUsage);

private:
    const size_t m_maxMemory;
    size_t m_usedMemory { 0 };

    using ItemsList = std::list<uint32_t>;
    ItemsList m_residentItems;
    std::vector<typename ItemsList::iterator> m_residentItemsLookUp;

    struct RefCountedItem {
        Evictable* pItem { nullptr };
        std::atomic_int refCount { 0 };
    };
    std::vector<RefCountedItem> m_items;
};

class EvictLRUCache::Builder {
public:
    template <typename T>
    CacheHandle<T> registerCacheable(T* pItem);

    EvictLRUCache build(size_t maxMemory);

private:
    std::vector<Evictable*> m_items;
};

template <typename T>
inline CachedPtr<T> EvictLRUCache::get(CacheHandle<T> handle)
{
    // NOTE: thread-safe only if get() is called from one thread at a time (ref count may be modified by any thread concurrently).
    RefCountedItem& refCountedItem = m_items[handle.index];
    Evictable* pItem = refCountedItem.pItem;
    if (pItem->isResident()) {
        // Remove from list and add to end
        auto& listIter = m_residentItemsLookUp[handle.index];
        m_residentItems.splice(std::end(m_residentItems), m_residentItems, listIter);
        listIter = --std::end(m_residentItems);

        return CachedPtr<T>(dynamic_cast<T*>(pItem), &refCountedItem.refCount);
    } else {
        assert(m_residentItemsLookUp[handle.index] == std::end(m_residentItems));

        size_t sizeBefore = pItem->sizeBytes();
        pItem->makeResident();
        assert(pItem->sizeBytes() >= sizeBefore);
        m_usedMemory += (pItem->sizeBytes() - sizeBefore);

        auto listIter = m_residentItems.insert(std::end(m_residentItems), handle.index);
        m_residentItemsLookUp[handle.index] = listIter;

        // Increase ref count before evict to make sure we never evict the item before returning
        auto result = CachedPtr<T>(dynamic_cast<T*>(pItem), &refCountedItem.refCount);
        if (m_usedMemory > m_maxMemory) {
            const size_t desiredFreeMemory = m_maxMemory / 4;
            evict(m_maxMemory - desiredFreeMemory);
        }
        return result;
    }
}

template <typename T>
inline CacheHandle<T> EvictLRUCache::Builder::registerCacheable(T* pItem)
{
    uint32_t index = static_cast<uint32_t>(m_items.size());
    m_items.push_back(pItem);
    return CacheHandle<T> { index };
}

}