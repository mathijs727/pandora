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
#include <gsl/span>
#include <unordered_map>
#include <vector>

namespace stream {

class LRUCache {
public:
    class Builder;
    template <typename T>
    CachedPtr<T> makeResident(T* pEvictable);

    size_t memoryUsage() const noexcept;
    bool checkResidencyIsValid() const;

private:
    LRUCache(std::unique_ptr<stream::Deserializer>&& pDeserializer, gsl::span<Evictable*> items, size_t maxMemory);

    void evict(size_t desiredMemoryUsage);

private:
    std::unique_ptr<stream::Deserializer> m_pDeserializer;

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
    std::unordered_map<Evictable*, uint32_t> m_pointerToItemIndex;
};

class LRUCache::Builder : public CacheBuilder {
public:
    Builder(std::unique_ptr<stream::Serializer>&& pSerializer);

    void registerCacheable(Evictable* pItem, bool evict = false);

    LRUCache build(size_t maxMemory);

private:
    std::unique_ptr<stream::Serializer> m_pSerializer;
    std::vector<Evictable*> m_items;
};

template <typename T>
inline CachedPtr<T> LRUCache::makeResident(T* pEvictable)
{
    // NOTE: thread-safe only if get() is called from one thread at a time (ref count may be modified by any thread concurrently).
    const uint32_t itemIndex = m_pointerToItemIndex[pEvictable];
    RefCountedItem& refCountedItem = m_items[itemIndex];
    Evictable* pItem = refCountedItem.pItem;
    if (pItem->isResident()) {
        // Remove from list and add to end
        auto& listIter = m_residentItemsLookUp[itemIndex];
        m_residentItems.splice(std::end(m_residentItems), m_residentItems, listIter);
        listIter = --std::end(m_residentItems);

        return CachedPtr<T>(dynamic_cast<T*>(pItem), &refCountedItem.refCount);
    } else {
        assert(m_residentItemsLookUp[itemIndex] == std::end(m_residentItems));

        size_t sizeBefore = pItem->sizeBytes();
        pItem->makeResident(*m_pDeserializer);
        assert(pItem->sizeBytes() >= sizeBefore);
        m_usedMemory += (pItem->sizeBytes() - sizeBefore);

        auto listIter = m_residentItems.insert(std::end(m_residentItems), itemIndex);
        m_residentItemsLookUp[itemIndex] = listIter;

        // Increase ref count before evict to make sure we never evict the item before returning
        auto result = CachedPtr<T>(dynamic_cast<T*>(pItem), &refCountedItem.refCount);
        if (m_usedMemory > m_maxMemory) {
            const size_t desiredFreeMemory = m_maxMemory / 4;
            evict(m_maxMemory - desiredFreeMemory);
        }
        return result;
    }
}

}