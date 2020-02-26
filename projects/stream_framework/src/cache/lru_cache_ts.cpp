#include "stream/cache/lru_cache_ts.h"
#include "enumerate.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace tasking {

LRUCacheTS::LRUCacheTS(std::unique_ptr<tasking::Deserializer>&& pDeserializer, gsl::span<Evictable*> items, size_t maxMemory)
    : m_pDeserializer(std::move(pDeserializer))
    , m_maxMemory(maxMemory)
{
    m_pItemData = std::make_unique<ItemData[]>(items.size());
    for (uint32_t i = 0; i < items.size(); i++) {
        m_itemDataIndices[items[i]] = i;
        if (items[i]->isResident())
            m_pItemData[i].state = ItemState::Loaded;
        m_usedMemory.fetch_add(items[i]->sizeBytes(), std::memory_order::memory_order_relaxed);
    }
}

LRUCacheTS& LRUCacheTS::operator=(LRUCacheTS&& other) noexcept
{
    m_pDeserializer = std::move(other.m_pDeserializer);
    m_maxMemory = other.m_maxMemory;
    m_usedMemory.store(other.m_usedMemory.load());

    m_pItemData = std::move(other.m_pItemData);
    m_itemDataIndices = std::move(other.m_itemDataIndices);
    return *this;
}

LRUCacheTS::~LRUCacheTS()
{
    spdlog::info("~LRUCacheTS(): memory usage = {} bytes", m_usedMemory.load());
    for (auto& [pItem, _] : m_itemDataIndices) {
        if (pItem->isResident())
            pItem->evict();
    }
}

size_t LRUCacheTS::memoryUsage() const noexcept
{
    return m_usedMemory;
}

size_t LRUCacheTS::maxSize() const
{
    return m_maxMemory;
}

void LRUCacheTS::forceEvict(Evictable* pEvictable)
{
    assert(pEvictable->isResident());

    auto& itemData = m_pItemData[m_itemDataIndices[pEvictable]];

    m_usedMemory -= pEvictable->sizeBytes();
    pEvictable->evict();
    m_usedMemory += pEvictable->sizeBytes();
    itemData.state = ItemState::Unloaded;
}

void LRUCacheTS::evictMarked()
{
    spdlog::debug("Evicting items from LRUCacheTS");

    std::lock_guard l { m_evictMutex };
    if (m_usedMemory.load(std::memory_order_relaxed) < m_maxMemory)
        return;

    // Evict marked items.
    int64_t freedMem { 0 };
    for (auto& [pItem, itemDataIndex] : m_itemDataIndices) {
        // NOTE: no other thread will try to load / use the item since the memory limit has been exceeded.
        auto& itemData = m_pItemData[itemDataIndex];
        const bool marked = itemData.marked.load(std::memory_order_relaxed);
        const ItemState state = itemData.state.load(std::memory_order_acquire);

        // If marked (not touched since last evict) and loaded in memory
        if (marked && state == ItemState::Loaded) {
            // Dont try to evict if someone (which includes us) is holding the item.
            if (itemData.refCount.load(std::memory_order_relaxed) != 0)
                continue;

            // Exchange state to evicting (if another thread requests the object it will wait for us to
            //  evict before starting to load it again).
            itemData.state.exchange(ItemState::Evicting);

            // Check again that no other thread accessed the object. If this is the case we need to continue
            // because that thread could also try to evict, which means it needs to wait for us (only a single
            // thread may evict at any time) and we need to wait for it to release the object => deadlock.
            if (itemData.refCount.load() != 0) {
                itemData.state.store(ItemState::Loaded, std::memory_order_acquire);
                continue;
            }

            freedMem += pItem->sizeBytes();
            pItem->evict();
            freedMem -= pItem->sizeBytes();

            itemData.state.store(ItemState::Unloaded, std::memory_order_release);
        }

        itemData.marked.store(true, std::memory_order_relaxed);
    }

    m_usedMemory.fetch_sub(freedMem, std::memory_order_relaxed);

    if (m_usedMemory.load(std::memory_order_relaxed) > m_maxMemory)
        spdlog::warn("LRUCacheTS: memory usage exceeded limit after eviction");
}

LRUCacheTS::Builder::Builder(std::unique_ptr<Serializer>&& pSerializer)
    : m_pSerializer(std::move(pSerializer))
{
}

void LRUCacheTS::Builder::registerCacheable(Evictable* pItem, bool evict)
{
    m_items.push_back(pItem);

    pItem->serialize(*m_pSerializer);

    if (pItem->isResident())
        pItem->evict();
}

LRUCacheTS LRUCacheTS::Builder::build(size_t maxMemory)
{
    return LRUCacheTS(m_pSerializer->createDeserializer(), m_items, maxMemory);
}

}