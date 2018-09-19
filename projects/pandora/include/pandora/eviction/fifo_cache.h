#pragma once
#include "pandora/core/pandora.h"
#include "pandora/eviction/evictable.h"
#include "pandora/geometry/triangle.h"
#include "pandora/utility/atomic_weak_ptr.h"
#include "pandora/utility/memory_arena.h"
#include <tbb/concurrent_queue.h>
#include <tbb/concurrent_unordered_map.h>
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
    FifoCache(size_t maxSizeBytes,
        const std::function<void(size_t)> allocCallback,
        const std::function<void(size_t)> evictCallback);

    // Hand of ownership of the resource to the cache
    EvictableResourceID emplaceFactoryUnsafe(std::function<T(void)> factoryFunc); // Not thread-safe

    std::shared_ptr<T> getBlocking(EvictableResourceID resourceID) const;

    void evictAllUnsafe() const;

private:
    void evict(size_t bytes);

private:
    const size_t m_maxSizeBytes;
    std::atomic_size_t m_currentSizeBytes;

    std::function<void(size_t)> m_allocCallback;
    std::function<void(size_t)> m_evictCallback;

    // This will grow indefinitely: bad!
    tbb::concurrent_queue<std::shared_ptr<T>> m_cacheHistory;
    //tbb::concurrent_unordered_map<EvictableResourceID, std::shared_ptr<T>> m_owningPointers;

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
    , m_currentSizeBytes(0)
    , m_allocCallback([](size_t) {})
    , m_evictCallback([](size_t) {})
{
}
template <typename T>
inline FifoCache<T>::FifoCache(
    size_t maxSizeBytes,
    const std::function<void(size_t)> allocCallback,
    const std::function<void(size_t)> evictCallback)
    : m_maxSizeBytes(maxSizeBytes)
    , m_currentSizeBytes(0)
    , m_allocCallback(allocCallback)
    , m_evictCallback(evictCallback)
{
}

template <typename T>
inline EvictableResourceID FifoCache<T>::emplaceFactoryUnsafe(std::function<T(void)> factoryFunc)
{
    auto resourceID = static_cast<EvictableResourceID>(m_resourceFactories.size());
    m_resourceFactories.push_back(factoryFunc);

    // Insert the key into the hashmap so that any "get" operations are read-only (and thus thread safe)
    m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());

    return resourceID;
}

template <typename T>
inline std::shared_ptr<T> FifoCache<T>::getBlocking(EvictableResourceID resourceID) const
{
    auto* mutThis = const_cast<FifoCache<T>*>(this);

    auto& cacheItem = mutThis->m_cacheMap[resourceID];
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
            mutThis->m_cacheHistory.push(sharedResourcePtr);
            m_allocCallback(resourceSize);

            size_t oldCacheSize = mutThis->m_currentSizeBytes.fetch_add(resourceSize);
            size_t newCacheSize = oldCacheSize + resourceSize;
            if (newCacheSize > m_maxSizeBytes) {
                // If another thread caused us to go over the memory limit that we only have to account
                //  for our own contribution.
                size_t overallocated = std::min(newCacheSize - m_maxSizeBytes, resourceSize);
                mutThis->evict(overallocated);
            }
        }
    }

    return sharedResourcePtr;
}

template <typename T>
inline void FifoCache<T>::evictAllUnsafe() const
{
    for (auto iter = m_cacheHistory.unsafe_begin(); iter != m_cacheHistory.unsafe_end(); iter++) {
        m_evictCallback((*iter)->sizeBytes());
    }

    auto* mutThis = const_cast<FifoCache<T>*>(this);
    mutThis->m_cacheHistory.clear();
    //mutThis->m_owningPointers.clear();
}

template <typename T>
inline void FifoCache<T>::evict(size_t bytesToEvict)
{

    size_t bytesEvicted = 0;
    while (bytesEvicted < bytesToEvict) {
        std::shared_ptr<T> sharedResourcePtr;

        if (m_cacheHistory.try_pop(sharedResourcePtr)) {
            bytesEvicted += sharedResourcePtr->sizeBytes();
        }
    }
    m_evictCallback(bytesEvicted);
    m_currentSizeBytes.fetch_sub(bytesEvicted);
}

}
