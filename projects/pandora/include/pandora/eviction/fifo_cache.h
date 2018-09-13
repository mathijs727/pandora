#pragma once
#include "pandora/core/pandora.h"
#include "pandora/eviction/evictable.h"
#include "pandora/geometry/triangle.h"
#include "pandora/utility/atomic_weak_ptr.h"
#include "pandora/utility/memory_arena.h"
#include <tbb/concurrent_queue.h>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace pandora {

using EvictableResourceID = uint32_t;

template <typename T>
class FifoCache {
public:
    FifoCache(size_t maxSizeBytes);

    // Hand of ownership of the resource to the cache
    EvictableResourceID emplaceFactoryUnsafe(std::function<T(void)> factoryFunc); // Not thread-safe

    void accessResource(EvictableResourceID resourceID, std::function<void(std::shared_ptr<T>)> callback);

private:
    void evict(size_t bytes);

private:
    const size_t m_maxSizeBytes;
    std::atomic_size_t m_currentSizeBytes;

    tbb::concurrent_queue<std::shared_ptr<T>> m_cacheHistory;

    struct CacheMapItem {
        std::mutex loadMutex;
        pandora::atomic_weak_ptr<T> itemPtr;
    };
    std::unordered_map<EvictableResourceID, CacheMapItem> m_cacheMap; // Read-only in the resource access function

    std::vector<std::function<T(void)>> m_resourceFactories;
    MemoryArena m_resourceFactoryAllocator;
};

template <typename T>
inline FifoCache<T>::FifoCache(size_t maxSizeBytes)
    : m_maxSizeBytes(maxSizeBytes)
{
}

template <typename T>
inline EvictableResourceID FifoCache<T>::emplaceFactoryUnsafe(std::function<T(void)> factoryFunc)
{
    auto resourceID = static_cast<EvictableResourceID>(m_resourceFactories.size());
    m_resourceFactories.push_back(factoryFunc);

    //m_cacheMap[resourceID] = std::weak_ptr<T>(); // Insert the key into the hashmap
    return resourceID;
}

template <typename T>
inline void FifoCache<T>::accessResource(EvictableResourceID resourceID, std::function<void(std::shared_ptr<T>)> callback)
{
    auto& cacheItem = m_cacheMap[resourceID];
    std::shared_ptr<T> sharedResourcePtr = cacheItem.itemPtr.lock();
    if (!sharedResourcePtr) {
        std::scoped_lock lock(cacheItem.loadMutex);

        // Make sure that no other thread came in first and loaded the resource already
        sharedResourcePtr = cacheItem.itemPtr.lock();
        if (!sharedResourcePtr) {
            // TODO: load data on a worker thread
            const auto& factoryFunc = m_resourceFactories[resourceID];
            sharedResourcePtr = std::make_shared<T>(factoryFunc());
            size_t resourceSize = sharedResourcePtr->sizeBytes();
            cacheItem.itemPtr.store(sharedResourcePtr);

            size_t oldCacheSize = m_currentSizeBytes.fetch_add(resourceSize);
            size_t newCacheSize = oldCacheSize + resourceSize;
            if (newCacheSize > m_maxSizeBytes) {
                // If another thread caused us to go over the memory limit that we only have to account
                //  for our own contribution.
                size_t overallocated = std::min(newCacheSize - m_maxSizeBytes, resourceSize);
                evict(overallocated);
            }
        }
    }

    callback(sharedResourcePtr);
}

template <typename T>
inline void FifoCache<T>::evict(size_t toEvict)
{
    size_t evicted = 0;
    while (evicted < toEvict) {
        std::shared_ptr<T> sharedResourcePtr;
        m_cacheHistory.try_pop(sharedResourcePtr);

        evicted += sharedResourcePtr->sizeBytes();
    }
    m_currentSizeBytes.fetch_sub(evicted);
}

}
