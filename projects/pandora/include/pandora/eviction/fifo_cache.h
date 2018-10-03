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
class FifoCache {
public:
    FifoCache(size_t maxSizeBytes, int loaderThreadCount);
    FifoCache(
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
        using LoadRequestData = std::tuple<S, CacheMapItem*, EvictableResourceID>;
        using AccessNode = tbb::flow::multifunction_node<FlowGraphInput, tbb::flow::tuple<FlowGraphOutput, LoadRequestData>>;
        using LoadNode = tbb::flow::async_node<LoadRequestData, FlowGraphOutput>;

        friend class FifoCache<T>;
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

    // This will grow indefinitely: bad!
    tbb::concurrent_queue<std::shared_ptr<T>> m_cacheHistory;
    //tbb::concurrent_unordered_map<EvictableResourceID, std::shared_ptr<T>> m_owningPointers;

public:
    // Has to be public because SubFlowGraph requires it as template parameter
    struct CacheMapItem {
        std::mutex loadMutex;
        pandora::atomic_weak_ptr<T> itemPtr;
    };

private:
    std::unordered_map<EvictableResourceID, CacheMapItem> m_cacheMap; // Read-only in the resource access function

    std::mutex m_factoryInsertMutex;
    std::vector<std::function<T(void)>> m_resourceFactories;
    ThreadPool m_factoryThreadPool;
};

template <typename T>
inline FifoCache<T>::FifoCache(size_t maxSizeBytes, int loaderThreadCount)
    : m_maxSizeBytes(maxSizeBytes)
    , m_currentSizeBytes(0)
    , m_allocCallback([](size_t) {})
    , m_evictCallback([](size_t) {})
    , m_factoryThreadPool(loaderThreadCount)
{
}
template <typename T>
inline FifoCache<T>::FifoCache(
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
inline EvictableResourceID FifoCache<T>::emplaceFactoryThreadSafe(std::function<T(void)> factoryFunc)
{
    std::scoped_lock<std::mutex> l(m_factoryInsertMutex);

    auto resourceID = static_cast<EvictableResourceID>(m_resourceFactories.size());
    m_resourceFactories.push_back(factoryFunc);

    // Insert the key into the hashmap so that any "get" operations are read-only (and thus thread safe)
    m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());

    return resourceID;
}


template <typename T>
inline bool FifoCache<T>::inCache(EvictableResourceID resourceID) const
{
    const auto& cacheItem = mutThis->m_cacheMap[resourceID];
    return !cacheItem.itemPtr.expired();
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
        } else {
            // Cache empty, stop evicting to prevent a deadlock.
            break;
        }
    }
    m_evictCallback(bytesEvicted);
    m_currentSizeBytes.fetch_sub(bytesEvicted);
}

template <typename T>
template <typename S>
inline typename FifoCache<T>::template SubFlowGraph<S> FifoCache<T>::getFlowGraphNode(tbb::flow::graph& g) const
{
    using Input = typename SubFlowGraph<S>::FlowGraphInput;
    using Output = typename SubFlowGraph<S>::FlowGraphOutput;
    using LoadRequestData = typename SubFlowGraph<S>::LoadRequestData;
    using AccessNode = typename SubFlowGraph<S>::AccessNode;
    using LoadNode = typename SubFlowGraph<S>::LoadNode;

    auto* mutThis = const_cast<FifoCache<T>*>(this);
    AccessNode accessNode(g, tbb::flow::unlimited, [mutThis, this](Input input, typename AccessNode::output_ports_type& op) {
        EvictableResourceID resourceID = std::get<1>(input);

        auto& cacheItem = mutThis->m_cacheMap[resourceID];
        std::shared_ptr<T> sharedResourcePtr = cacheItem.itemPtr.lock();
        if (sharedResourcePtr) {
            std::get<0>(op).try_put(std::make_pair(std::get<0>(input), sharedResourcePtr));
        } else {
            std::get<1>(op).try_put({ std::get<0>(input), &cacheItem, resourceID });
        }
    });
    LoadNode loadNode(g, tbb::flow::unlimited, [mutThis, this](const LoadRequestData& data, typename LoadNode::gateway_type& gatewayRef) {
        auto* gatewayPtr = &gatewayRef; // Work around for MSVC internal compiler error (when trying to capture gateway)
        gatewayPtr->reserve_wait();
        mutThis->m_factoryThreadPool.emplace([=]() {
            auto& cacheItem = *std::get<1>(data);
            std::scoped_lock lock(cacheItem.loadMutex);

            // Make sure that no other thread came in first and loaded the resource already
            auto sharedResourcePtr = cacheItem.itemPtr.lock();
            if (!sharedResourcePtr) {
                // Not mutating but having a hard time capturing this
                const auto& factoryFunc = m_resourceFactories[std::get<2>(data)];
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

            gatewayPtr->try_put(std::make_pair(std::get<0>(data), sharedResourcePtr));
            gatewayPtr->release_wait();
        });
    });
    return SubFlowGraph<S>(std::move(accessNode), std::move(loadNode));
}

template <typename T>
template <typename S>
template <typename N>
inline void FifoCache<T>::SubFlowGraph<S>::connectInput(N& inputNode)
{
    tbb::flow::make_edge(inputNode, m_accessNode);
}

template <typename T>
template <typename S>
template <typename N>
inline void FifoCache<T>::SubFlowGraph<S>::connectOutput(N& outputNode)
{
    tbb::flow::make_edge(tbb::flow::output_port<1>(m_accessNode), m_loadNode);

    tbb::flow::make_edge(tbb::flow::output_port<0>(m_accessNode), outputNode);
    tbb::flow::make_edge(m_loadNode, outputNode);
}

template <typename T>
template <typename S>
inline FifoCache<T>::SubFlowGraph<S>::SubFlowGraph(AccessNode&& accessNode, LoadNode&& loadNode)
    : m_accessNode(std::move(accessNode))
    , m_loadNode(std::move(loadNode))
{
}

}
