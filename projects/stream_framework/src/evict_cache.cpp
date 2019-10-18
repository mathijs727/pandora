#include "stream/evict_cache.h"
#include "enumerate.h"
#include <cassert>
#include <spdlog/spdlog.h>

namespace stream {

EvictLRUCache::EvictLRUCache(gsl::span<Evictable*> items, size_t maxMemory)
    : m_maxMemory(maxMemory)
    , m_items(items.size())
{
    for (auto [i, pItem] : enumerate(items)) {
        m_items[i].pItem = pItem;

        if (pItem->isResident()) {
            auto iter = m_residentItems.insert(std::end(m_residentItems), static_cast<uint32_t>(i));
            m_residentItemsLookUp.push_back(iter);
        } else {
            m_residentItemsLookUp.push_back(std::end(m_residentItems));
        }
        m_usedMemory += pItem->sizeBytes();
    }

    assert(checkResidencyIsValid());

    if (m_usedMemory > m_maxMemory) {
        spdlog::info("Evicting items in EvictLRUCache constructor");
        evict(m_maxMemory / 4 * 3);
    }
}

size_t EvictLRUCache::memoryUsage() const noexcept
{
    return m_usedMemory;
}

bool EvictLRUCache::checkResidencyIsValid() const
{
    for (uint32_t i : m_residentItems) {
        if (!m_items[i].pItem->isResident())
            return false;
    }

    size_t actualMemUsage = 0;
    for (size_t i = 0; i < m_items.size(); i++) {
        const auto* pItem = m_items[i].pItem;
        if (pItem->isResident()) {
            if (m_residentItemsLookUp[i] == std::end(m_residentItems))
                return false;
        } else {
            if (m_residentItemsLookUp[i] != std::end(m_residentItems))
                return false;
        }

        actualMemUsage += pItem->sizeBytes();
    }
    if (m_usedMemory != actualMemUsage)
        return false;

	return true;
}

void EvictLRUCache::evict(size_t desiredMemoryUsage)
{
    spdlog::info("Evicting items from EvictLRUCache");

	// Have to increment iterator before calling erase
    //for (auto iter = std::begin(m_residentItems); iter != std::end(m_residentItems); iter++) {
    auto iter = std::begin(m_residentItems);
    while (iter != std::end(m_residentItems)) {
        auto iterCopy = iter;
        uint32_t itemIndex = *iter++;
        auto& [pItem, refCount] = m_items[itemIndex];
        if (refCount.load(std::memory_order_relaxed) > 0)
            continue;

        assert(pItem->isResident());
        m_usedMemory -= pItem->sizeBytes();
        pItem->evict();
        m_usedMemory += pItem->sizeBytes();

        m_residentItems.erase(iterCopy);
        m_residentItemsLookUp[itemIndex] = std::end(m_residentItems);

        if (m_usedMemory <= desiredMemoryUsage)
            break;
    }

    if (m_usedMemory > desiredMemoryUsage) {
        spdlog::error("EvictLRUCache was not able to evict enough memory");
    }
}

EvictLRUCache EvictLRUCache::Builder::build(size_t maxMemory) {
    return EvictLRUCache(m_items, maxMemory);
}

}