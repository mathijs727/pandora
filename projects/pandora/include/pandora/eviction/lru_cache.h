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

template <typename T, typename... Args>
static constexpr bool contains = true; // (std::is_same<T, Args> || ...);

namespace pandora {

using EvictableResourceID = uint32_t;

template <typename... T>
class LRUCache {
public:
    LRUCache(size_t maxSizeBytes, int loaderThreadCount);
    LRUCache(
        size_t maxSizeBytes,
        int loaderThreadCount,
        const std::function<void(size_t)> allocCallback,
        const std::function<void(size_t)> evictCallback);

    // Tell the cache how to construct item
    template <typename S>
    std::enable_if_t<contains<S, T...>, EvictableResourceID> emplaceFactoryUnsafe(std::function<S(void)> factoryFunc); // Not thread-safe
    template <typename S>
    std::enable_if_t<contains<S, T...>, EvictableResourceID> emplaceFactoryThreadSafe(std::function<S(void)> factoryFunc); // Not thread-safe

    bool inCache(EvictableResourceID resourceID) const;

    template <typename S>
    std::shared_ptr<S> getBlocking(EvictableResourceID resourceID) const;

    // Output may be in a different order than the input so the node should also store any associated user data

    using HistoryType = std::list<std::pair<EvictableResourceID, std::variant<std::shared_ptr<T>...>>>;

    struct CacheMapItem;
    template <typename UserState, typename ItemType>
    struct SubFlowGraph {
    public:
        using FlowGraphInput = std::pair<UserState, EvictableResourceID>;
        using FlowGraphOutput = std::pair<UserState, std::shared_ptr<ItemType>>;

    public:
        SubFlowGraph(SubFlowGraph&&) = default;

        template <typename N>
        void connectInput(N& inputNode);

        template <typename N>
        void connectOutput(N& outputNode);

    private:
        // CacheMapItem pointer because using a reference
        using HistoryIterator = typename HistoryType::iterator;
        using LoadRequestData = std::tuple<UserState, std::pair<CacheMapItem, HistoryIterator>*, EvictableResourceID>;
        using AccessNode = tbb::flow::multifunction_node<FlowGraphInput, tbb::flow::tuple<FlowGraphOutput, LoadRequestData>>;
        using LoadNode = tbb::flow::async_node<LoadRequestData, FlowGraphOutput>;

        friend class LRUCache<T...>;
        SubFlowGraph(AccessNode&& accessNode, LoadNode&& loadNode);

    private:
        AccessNode m_accessNode;
        LoadNode m_loadNode;
    };
    template <typename UserState, typename ItemType>
    SubFlowGraph<UserState, ItemType> getFlowGraphNode(tbb::flow::graph& g) const;

    void evictAllUnsafe() const;

private:
    void evict(size_t bytes) const;

private:
    const size_t m_maxSizeBytes;
    mutable std::atomic_size_t m_currentSizeBytes;

    std::function<void(size_t)> m_allocCallback;
    std::function<void(size_t)> m_evictCallback;

    mutable std::mutex m_cacheMutex;
    mutable HistoryType m_history;

public:
    // Public because the SubFlowGraph has to refer to it
    struct CacheMapItem {
        template <typename S>
        static CacheMapItem construct()
        {
            return CacheMapItem { {}, pandora::atomic_weak_ptr<S>(nullptr) };
        }
        std::mutex loadMutex;
        std::variant<pandora::atomic_weak_ptr<T>...> itemPtr;
    };

private:
    using HistoryIterator = typename decltype(m_history)::iterator;
    mutable std::unordered_map<EvictableResourceID, std::pair<CacheMapItem, HistoryIterator>> m_cacheMap;
    std::vector<std::variant<std::function<T(void)>...>> m_resourceFactories;

    mutable ThreadPool m_factoryThreadPool;
};

template <typename... T>
inline LRUCache<T...>::LRUCache(size_t maxSizeBytes, int loaderThreadCount)
    : m_maxSizeBytes(maxSizeBytes)
    , m_currentSizeBytes(0)
    , m_allocCallback([](size_t) {})
    , m_evictCallback([](size_t) {})
    , m_factoryThreadPool(loaderThreadCount)
{
}
template <typename... T>
inline LRUCache<T...>::LRUCache(
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

template <typename... T>
template <typename S>
inline std::enable_if_t<contains<S, T...>, EvictableResourceID>
LRUCache<T...>::emplaceFactoryUnsafe(std::function<S(void)> factoryFunc)
{
    EvictableResourceID resourceID = static_cast<EvictableResourceID>(m_resourceFactories.size());
    m_resourceFactories.push_back(factoryFunc);

    // Create an empty cache item
    auto[cacheMapIter, success] = m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());
    auto& cacheMapItem = cacheMapIter->second;
    cacheMapItem.first.itemPtr.template emplace<pandora::atomic_weak_ptr<S>>();// Initialize variant with the correct type
    cacheMapItem.second = m_history.end();

    return resourceID;
}

template <typename... T>
template <typename S>
inline std::enable_if_t<contains<S, T...>, EvictableResourceID>
LRUCache<T...>::emplaceFactoryThreadSafe(std::function<S(void)> factoryFunc)
{
    std::scoped_lock l(m_cacheMutex);

    EvictableResourceID resourceID = static_cast<EvictableResourceID>(m_resourceFactories.size());
    m_resourceFactories.push_back(factoryFunc);

    // Create an empty cache item
    auto[cacheMapIter, success] = m_cacheMap.emplace(std::piecewise_construct,
        std::forward_as_tuple(resourceID),
        std::forward_as_tuple());
    auto& cacheMapItem = cacheMapIter->second;
    cacheMapItem.first.itemPtr.template emplace<pandora::atomic_weak_ptr<S>>();// Initialize variant with the correct type
    cacheMapItem.second = m_history.end();

    return resourceID;
}

template <typename... T>
inline bool LRUCache<T...>::inCache(EvictableResourceID resourceID) const
{
    const auto& [cacheItem, lruIter] = m_cacheMap.find(resourceID)->second;
    return std::visit([](auto&& itemSharedPtr) -> bool { return !itemSharedPtr.expired(); }, cacheItem.itemPtr);
}

template <typename... T>
template <typename S>
inline std::shared_ptr<S> LRUCache<T...>::getBlocking(EvictableResourceID resourceID) const
{
    auto& [cacheItem, lruIter] = m_cacheMap.find(resourceID)->second;
    auto& cacheMapWeakPtr = std::get<pandora::atomic_weak_ptr<S>>(cacheItem.itemPtr);
    std::shared_ptr<S> sharedResourcePtr = cacheMapWeakPtr.lock();
    if (!sharedResourcePtr) {
        std::scoped_lock itemLock(cacheItem.loadMutex);

        // Make sure that no other thread came in first and loaded the resource already
        sharedResourcePtr = cacheMapWeakPtr.lock();
        if (!sharedResourcePtr) {
            const auto& factoryFunc = std::get<std::function<S(void)>>(m_resourceFactories[resourceID]);
            sharedResourcePtr = std::make_shared<S>(factoryFunc());
            size_t resourceSize = sharedResourcePtr->sizeBytes();
            cacheMapWeakPtr.store(sharedResourcePtr);
            {
                std::scoped_lock cacheLock(m_cacheMutex);
                lruIter = m_history.insert(std::begin(m_history), { resourceID, sharedResourcePtr });
            }
            m_allocCallback(resourceSize);

            size_t oldCacheSize = m_currentSizeBytes.fetch_add(resourceSize);
            size_t newCacheSize = oldCacheSize + resourceSize;
            if (newCacheSize > m_maxSizeBytes) {
                //std::cout << "LRU cache evicting memory, new size: " << newCacheSize << ", limit: " << m_maxSizeBytes << std::endl;

                // If another thread caused us to go over the memory limit that we only have to account
                //  for our own contribution.
                size_t overallocated = std::min(newCacheSize - m_maxSizeBytes, resourceSize);
                evict(overallocated);
            }
        }
    } else {
        // The item was already in cache and not just loaded (either by us or another thread).
        // Move the item to the front of the history list.
        std::scoped_lock cacheLock(m_cacheMutex);
        if (lruIter != m_history.end()) {
            m_history.erase(lruIter);
        }
        lruIter = m_history.insert(std::begin(m_history), { resourceID, sharedResourcePtr });
    }

    // TODO: move item back to the front of the list
    return sharedResourcePtr;
}

template <typename... T>
inline void LRUCache<T...>::evictAllUnsafe() const
{
    while (!m_history.empty()) {
        auto [resourceID, sharedResourcePtrVariant] = m_history.back();
        m_history.pop_back();

        // 1. second -> select value from key/value pair returned by m_cacheMap.find()
        // 2. second -> select history iterator from (CacheItem, iterator) pair
        m_cacheMap.find(resourceID)->second.second = m_history.end();

        m_evictCallback(std::visit([](auto&& sharedResourcePtr) -> size_t { return sharedResourcePtr->sizeBytes(); }, sharedResourcePtrVariant));
    }
    m_currentSizeBytes.store(0);
}

template <typename... T>
inline void LRUCache<T...>::evict(size_t bytesToEvict) const
{
    std::scoped_lock l(m_cacheMutex);

    size_t bytesEvicted = 0;
    while (bytesEvicted < bytesToEvict) {
        // Cache empty, stop evicting to prevent a deadlock.
        if (m_history.empty())
            break;

        auto [resourceID, sharedResourcePtrVariant] = m_history.back();
        m_history.pop_back();

        // 1. second -> select value from key/value pair returned by m_cacheMap.find()
        // 2. second -> select history iterator from (CacheItem, iterator) pair
        m_cacheMap.find(resourceID)->second.second = m_history.end();

        bytesEvicted += std::visit([](auto&& sharedResourcePtr) -> size_t { return sharedResourcePtr->sizeBytes(); }, sharedResourcePtrVariant);
    }
    m_evictCallback(bytesEvicted);
    m_currentSizeBytes.fetch_sub(bytesEvicted);

    //std::cout << "Evicted " << bytesEvicted << " to keep memory usage in check" << std::endl;
}

template <typename... T>
template <typename UserState, typename ItemType>
inline typename LRUCache<T...>::template SubFlowGraph<UserState, ItemType> LRUCache<T...>::getFlowGraphNode(tbb::flow::graph& g) const
{
    using Input = typename SubFlowGraph<UserState, ItemType>::FlowGraphInput;
    //using Output = typename SubFlowGraph<S>::FlowGraphOutput;
    using LoadRequestData = typename SubFlowGraph<UserState, ItemType>::LoadRequestData;
    using AccessNode = typename SubFlowGraph<UserState, ItemType>::AccessNode;
    using LoadNode = typename SubFlowGraph<UserState, ItemType>::LoadNode;

    AccessNode accessNode(g, tbb::flow::unlimited, [this](Input input, typename AccessNode::output_ports_type& op) {
        EvictableResourceID resourceID = std::get<1>(input);

        auto& cacheItemIterPair = m_cacheMap.find(resourceID)->second;
        auto& cacheItem = std::get<0>(cacheItemIterPair);
        auto& cacheMapWeakPtr = std::get<pandora::atomic_weak_ptr<ItemType>>(cacheItem.itemPtr);
        std::shared_ptr<ItemType> sharedResourcePtr = cacheMapWeakPtr.lock();
        if (sharedResourcePtr) {
            std::get<0>(op).try_put({ std::get<0>(input), sharedResourcePtr });
        } else {
            std::get<1>(op).try_put({ std::get<0>(input), &cacheItemIterPair, resourceID });
        }
    });
    LoadNode loadNode(g, tbb::flow::unlimited, [this](const LoadRequestData& data, typename LoadNode::gateway_type& gatewayRef) {
        auto* gatewayPtr = &gatewayRef; // Work around for MSVC internal compiler error (when trying to capture gateway)
        gatewayPtr->reserve_wait();
        m_factoryThreadPool.emplace([=]() {
            auto& [cacheItem, lruIter] = *std::get<1>(data);
            auto& cacheMapWeakPtr = std::get<pandora::atomic_weak_ptr<ItemType>>(cacheItem.itemPtr);
            auto resourceID = std::get<2>(data);

            auto sharedResourcePtr = cacheMapWeakPtr.lock();
            if (!sharedResourcePtr) {
                std::scoped_lock itemLock(cacheItem.loadMutex);

                // Make sure that no other thread came in first and loaded the resource already
                sharedResourcePtr = cacheMapWeakPtr.lock();
                if (!sharedResourcePtr) {
                    // Not mutating but having a hard time capturing "this" pointer
                    const auto& factoryFunc = std::get<std::function<ItemType(void)>>(m_resourceFactories[resourceID]);
                    sharedResourcePtr = std::make_shared<ItemType>(factoryFunc());
                    size_t resourceSize = sharedResourcePtr->sizeBytes();
                    cacheMapWeakPtr.store(sharedResourcePtr);
                    {
                        std::scoped_lock cacheLock(m_cacheMutex);
                        lruIter = m_history.insert(std::begin(m_history), { resourceID, sharedResourcePtr });
                    }
                    m_allocCallback(resourceSize);

                    size_t oldCacheSize = m_currentSizeBytes.fetch_add(resourceSize);
                    size_t newCacheSize = oldCacheSize + resourceSize;
                    if (newCacheSize > m_maxSizeBytes) {
                        // If another thread caused us to go over the memory limit that we only have to account
                        //  for our own contribution.
                        size_t overallocated = std::min(newCacheSize - m_maxSizeBytes, resourceSize);
                        evict(overallocated);
                    }
                }
            } else {
                // The item was already in cache and not just loaded (either by us or another thread).
                // Move the item to the front of the history list.
                std::scoped_lock cacheLock(m_cacheMutex);
                if (lruIter != m_history.end()) {
                    m_history.erase(lruIter);
                }
                lruIter = m_history.insert(std::begin(m_history), { resourceID, sharedResourcePtr });
            }

            gatewayPtr->try_put(std::make_pair(std::get<0>(data), sharedResourcePtr));
            gatewayPtr->release_wait();
        });
    });
    return SubFlowGraph<UserState, ItemType>(std::move(accessNode), std::move(loadNode));
}

template <typename... T>
template <typename UserState, typename ItemType>
template <typename N>
inline void LRUCache<T...>::SubFlowGraph<UserState, ItemType>::connectInput(N& inputNode)
{
    tbb::flow::make_edge(inputNode, m_accessNode);
}

template <typename... T>
template <typename UserState, typename ItemType>
template <typename N>
inline void LRUCache<T...>::SubFlowGraph<UserState, ItemType>::connectOutput(N& outputNode)
{
    tbb::flow::make_edge(tbb::flow::output_port<1>(m_accessNode), m_loadNode);

    tbb::flow::make_edge(tbb::flow::output_port<0>(m_accessNode), outputNode);
    tbb::flow::make_edge(m_loadNode, outputNode);
}

template <typename... T>
template <typename UserState, typename ItemType>
inline LRUCache<T...>::SubFlowGraph<UserState, ItemType>::SubFlowGraph(AccessNode&& accessNode, LoadNode&& loadNode)
    : m_accessNode(std::move(accessNode))
    , m_loadNode(std::move(loadNode))
{
}
}
