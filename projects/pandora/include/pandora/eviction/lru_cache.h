#pragma once
#include "pandora/core/pandora.h"
#include "pandora/eviction/evictable.h"
#include "pandora/utility/atomic_weak_ptr.h"
#include "pandora/utility/thread_pool.h"
#include <mutex>
#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pandora {

using EvictableResourceID = uint32_t;

template <typename T>
class LRUCache {
public:
    LRUCache(size_t maxSizeBytes, int loaderThreadCount);
    LRUCache(
        size_t maxSizeBytes,
        int loaderThreadCount,
        const std::function<void(size_t)> allocCallback,
        const std::function<void(size_t)> evictCallback);

    // Hand of ownership of the resource to the cache
    EvictableResourceID emplaceFactoryUnsafe(std::function<T(void)> factoryFunc); // Not thread-safe
    EvictableResourceID emplaceFactoryThreadSafe(std::function<T(void)> factoryFunc); // Not thread-safe

    bool inCache(EvictableResourceID resourceID) const;

    std::shared_ptr<T> getBlocking(EvictableResourceID resourceID) const;

    // Output may be in a different order than the input so the node should also store any associated user data

    struct CacheMapItem;
    template <typename S>
    struct SubFlowGraph {
    public:
        using FlowGraphInput = std::pair<S, EvictableResourceID>;
        using FlowGraphOutput = std::pair<S, std::shared_ptr<T>>;

    public:
        SubFlowGraph(SubFlowGraph&&) = default;

        template <typename N>
        void connectInput(N& inputNode);

        template <typename N>
        void connectOutput(N& outputNode);

    private:
        // CacheMapItem pointer because using a reference
        using HistoryIterator = typename std::list<std::shared_ptr<T>>::iterator;
        using LoadRequestData = std::tuple<S, std::pair<CacheMapItem, HistoryIterator>*, EvictableResourceID>;
        using AccessNode = tbb::flow::multifunction_node<FlowGraphInput, tbb::flow::tuple<FlowGraphOutput, LoadRequestData>>;
        using LoadNode = tbb::flow::async_node<LoadRequestData, FlowGraphOutput>;

        friend class LRUCache<T>;
        SubFlowGraph(AccessNode&& accessNode, LoadNode&& loadNode);

    private:
        AccessNode m_accessNode;
        LoadNode m_loadNode;
    };
    template <typename S>
    SubFlowGraph<S> getFlowGraphNode(tbb::flow::graph& g) const;

    void evictAllUnsafe() const;

private:
    void evict(size_t bytes);

private:
    const size_t m_maxSizeBytes;
    std::atomic_size_t m_currentSizeBytes;

    std::function<void(size_t)> m_allocCallback;
    std::function<void(size_t)> m_evictCallback;

    std::mutex m_cacheMutex;
    std::list<std::shared_ptr<T>> m_history;
public:
    struct CacheMapItem {
        std::mutex loadMutex;
        pandora::atomic_weak_ptr<T> itemPtr;
    };
private:
    using HistoryIterator = typename std::list<std::shared_ptr<T>>::iterator;
    std::unordered_map<EvictableResourceID, std::pair<CacheMapItem, HistoryIterator>> m_cacheMap; // Read-only in the resource access function

    tbb::concurrent_vector<std::function<T(void)>> m_resourceFactories;
    ThreadPool m_factoryThreadPool;
};

template <typename T>
inline LRUCache<T>::LRUCache(size_t maxSizeBytes, int loaderThreadCount)
    : m_maxSizeBytes(maxSizeBytes)
    , m_currentSizeBytes(0)
    , m_allocCallback([](size_t) {})
    , m_evictCallback([](size_t) {})
    , m_factoryThreadPool(loaderThreadCount)
{
}
template <typename T>
inline LRUCache<T>::LRUCache(
    size_t maxSizeBytes,
    int loaderThreadCount,
    const std::function<void(size_t)> allocCallback,
    const std::function<void(size_t)> evictCallback)
    : m_maxSizeBytes(maxSizeBytes)
    , m_currentSizeBytes(0)
    , m_allocCallback(allocCallback)
    , m_evictCallback(evictCallback)
    , m_factoryThreadPool(loaderThreadCount)
{
}

template <typename T>
inline EvictableResourceID LRUCache<T>::emplaceFactoryUnsafe(std::function<T(void)> factoryFunc)
{
    auto iter = m_resourceFactories.push_back(factoryFunc);
    EvictableResourceID resourceID = iter - m_resourceFactories.begin();

    // Insert the key into the hashmap so that any "get" operations are read-only (and thus thread safe)
    m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());
    m_cacheMap[resourceID].second = m_history.end();

    return resourceID;
}

template <typename T>
inline EvictableResourceID LRUCache<T>::emplaceFactoryThreadSafe(std::function<T(void)> factoryFunc)
{
    auto iter = m_resourceFactories.push_back(factoryFunc);
    EvictableResourceID resourceID = iter - m_resourceFactories.begin();

    // Insert the key into the hashmap so that any "get" operations are read-only (and thus thread safe)
    std::scoped_lock l(m_cacheMutex);
    m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());
    m_cacheMap[resourceID].second = m_history.end();

    return resourceID;
}

template <typename T>
inline bool LRUCache<T>::inCache(EvictableResourceID resourceID) const
{
    const auto&[cacheItem, lruIter] = m_cacheMap.find(resourceID)->second;
    return !cacheItem.itemPtr.expired();
}

template <typename T>
inline std::shared_ptr<T> LRUCache<T>::getBlocking(EvictableResourceID resourceID) const
{
    auto* mutThis = const_cast<LRUCache<T>*>(this);

    auto& [cacheItem, lruIter] = mutThis->m_cacheMap[resourceID];
    std::shared_ptr<T> sharedResourcePtr = cacheItem.itemPtr.lock();
    if (!sharedResourcePtr) {
        std::scoped_lock itemLock(cacheItem.loadMutex);

        // Make sure that no other thread came in first and loaded the resource already
        sharedResourcePtr = cacheItem.itemPtr.lock();
        if (!sharedResourcePtr) {
            ALWAYS_ASSERT(lruIter == m_history.end());

            const auto& factoryFunc = m_resourceFactories[resourceID];
            sharedResourcePtr = std::make_shared<T>(factoryFunc());
            size_t resourceSize = sharedResourcePtr->sizeBytes();
            cacheItem.itemPtr.store(sharedResourcePtr);
            {
                std::scoped_lock cacheLock(mutThis->m_cacheMutex);
                lruIter = mutThis->m_history.insert(std::begin(m_history), sharedResourcePtr);
            }
            m_allocCallback(resourceSize);

            size_t oldCacheSize = mutThis->m_currentSizeBytes.fetch_add(resourceSize);
            size_t newCacheSize = oldCacheSize + resourceSize;
            if (newCacheSize > m_maxSizeBytes) {
                std::cout << "LRU cache evicting memory, new size: " << newCacheSize << ", limit: " << m_maxSizeBytes << std::endl;

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
inline void LRUCache<T>::evictAllUnsafe() const
{
    for (auto iter = this->m_cacheHistory.unsafe_begin(); iter != this->m_cacheHistory.unsafe_end(); iter++) {
        m_evictCallback((*iter)->sizeBytes());
    }

    auto* mutThis = const_cast<LRUCache<T>*>(this);
    mutThis->m_history.clear();
}

template <typename T>
inline void LRUCache<T>::evict(size_t bytesToEvict)
{
    std::scoped_lock l(m_cacheMutex);

    size_t bytesEvicted = 0;
    while (bytesEvicted < bytesToEvict) {
        // Cache empty, stop evicting to prevent a deadlock.
        if (m_history.empty())
            break;

        auto sharedResourcePtr = m_history.back();
        m_history.pop_back();

        bytesEvicted += sharedResourcePtr->sizeBytes();
    }
    m_evictCallback(bytesEvicted);
    m_currentSizeBytes.fetch_sub(bytesEvicted);
}

template <typename T>
template <typename S>
inline typename LRUCache<T>::template SubFlowGraph<S> LRUCache<T>::getFlowGraphNode(tbb::flow::graph& g) const
{
    using Input = typename SubFlowGraph<S>::FlowGraphInput;
    //using Output = typename SubFlowGraph<S>::FlowGraphOutput;
    using LoadRequestData = typename SubFlowGraph<S>::LoadRequestData;
    using AccessNode = typename SubFlowGraph<S>::AccessNode;
    using LoadNode = typename SubFlowGraph<S>::LoadNode;

    auto* mutThis = const_cast<LRUCache<T>*>(this);
    AccessNode accessNode(g, tbb::flow::unlimited, [mutThis, this](Input input, typename AccessNode::output_ports_type& op) {
        EvictableResourceID resourceID = std::get<1>(input);

        auto& cacheItemIterPair = mutThis->m_cacheMap[resourceID];
        std::shared_ptr<T> sharedResourcePtr = std::get<0>(cacheItemIterPair).itemPtr.lock();
        if (sharedResourcePtr) {
            std::get<0>(op).try_put({ std::get<0>(input), sharedResourcePtr });
        } else {
            std::get<1>(op).try_put({ std::get<0>(input), &cacheItemIterPair, resourceID });
        }
    });
    LoadNode loadNode(g, tbb::flow::unlimited, [mutThis, this](const LoadRequestData& data, typename LoadNode::gateway_type& gatewayRef) {
        auto* gatewayPtr = &gatewayRef; // Work around for MSVC internal compiler error (when trying to capture gateway)
        gatewayPtr->reserve_wait();
        mutThis->m_factoryThreadPool.emplace([=]() {
            auto& [cacheItem, lruIter] = *std::get<1>(data);
            std::scoped_lock itemLock(cacheItem.loadMutex);

            // Make sure that no other thread came in first and loaded the resource already
            auto sharedResourcePtr = cacheItem.itemPtr.lock();
            if (!sharedResourcePtr) {
                // Not mutating but having a hard time capturing "this" pointer
                const auto& factoryFunc = m_resourceFactories[std::get<2>(data)];
                sharedResourcePtr = std::make_shared<T>(factoryFunc());
                size_t resourceSize = sharedResourcePtr->sizeBytes();
                cacheItem.itemPtr.store(sharedResourcePtr);
                {
                    std::scoped_lock cacheLock(mutThis->m_cacheMutex);
                    lruIter = mutThis->m_history.insert(std::begin(m_history), sharedResourcePtr);
                }
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

            gatewayPtr->try_put(std::make_pair(std::get<0>(data), sharedResourcePtr));
            gatewayPtr->release_wait();
        });
    });
    return SubFlowGraph<S>(std::move(accessNode), std::move(loadNode));
}

template <typename T>
template <typename S>
template <typename N>
inline void LRUCache<T>::SubFlowGraph<S>::connectInput(N& inputNode)
{
    tbb::flow::make_edge(inputNode, m_accessNode);
}

template <typename T>
template <typename S>
template <typename N>
inline void LRUCache<T>::SubFlowGraph<S>::connectOutput(N& outputNode)
{
    tbb::flow::make_edge(tbb::flow::output_port<1>(m_accessNode), m_loadNode);

    tbb::flow::make_edge(tbb::flow::output_port<0>(m_accessNode), outputNode);
    tbb::flow::make_edge(m_loadNode, outputNode);
}

template <typename T>
template <typename S>
inline LRUCache<T>::SubFlowGraph<S>::SubFlowGraph(AccessNode&& accessNode, LoadNode&& loadNode)
    : m_accessNode(std::move(accessNode))
    , m_loadNode(std::move(loadNode))
{
}

}
