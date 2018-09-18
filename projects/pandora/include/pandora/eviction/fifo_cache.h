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
#include <tbb/concurrent_unordered_map.h>
#include <vector>

namespace pandora {

using EvictableResourceID = uint32_t;

template <typename T>
class FifoCache {
public:
    FifoCache(size_t maxSizeBytes);

    // Hand of ownership of the resource to the cache
    EvictableResourceID emplaceFactoryUnsafe(std::function<T(void)> factoryFunc); // Not thread-safe

    std::shared_ptr<T> getBlocking(EvictableResourceID resourceID) const;

    void evictAll() const;

private:
    void evict(size_t bytes);

private:
    const size_t m_maxSizeBytes;
    std::atomic_size_t m_currentSizeBytes;

    // This will grow indefinitely: bad!
    //tbb::concurrent_queue<std::shared_ptr<T>> m_cacheHistory;
    
    tbb::concurrent_unordered_map<EvictableResourceID, std::shared_ptr<T>> m_owningPointers;

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

    ALWAYS_ASSERT(m_cacheMap.find(resourceID) != m_cacheMap.end());
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
            size_t resourceSize = sharedResourcePtr->size();
            cacheItem.itemPtr.store(sharedResourcePtr);
            ALWAYS_ASSERT(m_owningPointers.find(resourceID) == m_owningPointers.end());
            ALWAYS_ASSERT(mutThis->m_cacheMap[resourceID].itemPtr.lock() == sharedResourcePtr);
            //m_cacheHistory.push(sharedResourcePtr);
            mutThis->m_owningPointers[resourceID] = sharedResourcePtr;

            size_t oldCacheSize = mutThis->m_currentSizeBytes.fetch_add(resourceSize);
            size_t newCacheSize = oldCacheSize + resourceSize;
            if (newCacheSize > m_maxSizeBytes) {
                // If another thread caused us to go over the memory limit that we only have to account
                //  for our own contribution.
                size_t overallocated = std::min(newCacheSize - m_maxSizeBytes, resourceSize);
                //std::cout << "Current cache size: " << newCacheSize << std::endl;
                mutThis->evict(overallocated);
            }
        }
    }

    return sharedResourcePtr;
}

template <typename T>
inline void FifoCache<T>::evictAll() const
{
    auto* mutThis = const_cast<FifoCache<T>*>(this);
    //mutThis->m_cacheHistory.clear();
    mutThis->m_owningPointers.clear();
}

template <typename T>
inline void FifoCache<T>::evict(size_t bytesToEvict)
{
    /*size_t bytesEvicted = 0;
    while (bytesEvicted < bytesToEvict) {
        std::shared_ptr<T> sharedResourcePtr;
        m_cacheHistory.try_pop(sharedResourcePtr);

        bytesEvicted += sharedResourcePtr->size();
    }
    m_currentSizeBytes.fetch_sub(bytesEvicted);*/
    THROW_ERROR("Not implemented");
}

}
