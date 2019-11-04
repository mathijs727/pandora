#pragma once
#include "handle.h"
#include <cassert>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <list>
#include <memory>
#include <spdlog/spdlog.h>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace tasking {

template <typename T>
class LRUCache {
public:
    class Builder;
    std::shared_ptr<T> get(CacheHandle<T> handle);

    size_t memoryUsage() const;

private:
    LRUCache(size_t maxMemory, std::vector<std::filesystem::path>&& constructData);

    std::shared_ptr<T> load(CacheHandle<T> handle);
    void evict(size_t minFreeMemory);

private:
    using ItemsList = std::list<uint32_t>;
    ItemsList m_inMemoryItems;
    std::unordered_map<uint32_t, std::pair<std::shared_ptr<T>, typename ItemsList::iterator>> m_lookUp;
    std::unordered_map<uint32_t, std::pair<std::weak_ptr<T>, size_t>> m_failEvictItems; // Items that were actively in use when trying to evict them. Might still be in memory and ready for re-use

    const size_t m_maxMemory;
    size_t m_usedMemory { 0 };
    std::vector<std::filesystem::path> m_constructData;
};

template <typename T>
class LRUCache<T>::Builder {
public:
    CacheHandle<T> registerCacheable(const T& item);
    LRUCache<T> build(size_t maxMemory);

private:
    std::vector<std::filesystem::path> m_constructData;
};

template <typename T>
inline std::shared_ptr<T> LRUCache<T>::get(CacheHandle<T> handle)
{
    // Look up
    if (auto mapIter = m_lookUp.find(handle.index); mapIter != std::end(m_lookUp)) {
        auto [pItem, listIter] = mapIter->second;

        // Remove from list and add to end
        m_inMemoryItems.splice(std::end(m_inMemoryItems), m_inMemoryItems, listIter);
        listIter = --std::end(m_inMemoryItems);
        mapIter->second.second = listIter;

        return pItem;
    } else {
        return load(handle);
    }
}

template <typename T>
inline size_t LRUCache<T>::memoryUsage() const
{
    return m_usedMemory;
}

template <typename T>
inline LRUCache<T>::LRUCache(size_t maxMemory, std::vector<std::filesystem::path>&& constructData)
    : m_maxMemory(maxMemory)
    , m_constructData(constructData)
{
}

template <typename T>
inline std::shared_ptr<T> LRUCache<T>::load(CacheHandle<T> handle)
{
    // Recycle item that was evicted but hold in memory by the user
    if (auto iter = m_failEvictItems.find(handle.index); iter != std::end(m_failEvictItems)) {
        std::weak_ptr<T> pItemWeak = iter->second.first;
        std::shared_ptr<T> pItem = pItemWeak.lock();
        if (pItem) {
            m_failEvictItems.erase(iter);
            return pItem;
        }
    }

    std::vector<std::byte> data;
    std::ifstream is { m_constructData[handle.index], std::ios::binary };

    // Initialize cursor at end of file
    is.seekg(0, std::ios_base::end);
    std::streampos fileSize = is.tellg();
    data.resize(fileSize);

    is.seekg(0, std::ios_base::beg);
    is.read(reinterpret_cast<char*>(data.data()), fileSize);

    // Create instance from loaded bytes
    auto pItem = std::make_shared<T>(data);
    m_usedMemory += pItem->sizeBytes();

    // Free a quarter of the memory budget so we don't have to evict all the time.
    if (m_usedMemory > m_maxMemory) {
        evict(std::min(pItem->sizeBytes(), m_usedMemory / 4));
    }

    // Insert into cache
    m_inMemoryItems.emplace_back(handle.index);
    m_lookUp[handle.index] = std::pair { pItem, --std::end(m_inMemoryItems) };
    assert(m_inMemoryItems.size() <= m_constructData.size()); // Can never have more items in cache than there are registered.

    return pItem;
}

template <typename T>
inline void LRUCache<T>::evict(size_t minFreeMemory)
{
    {
        std::vector<uint32_t> evicted;
        for (auto&& [handleIndex, itemInfo] : m_failEvictItems) {
            auto [pItemWeak, itemSize] = itemInfo;
            if (pItemWeak.expired()) {
                m_usedMemory -= itemSize;
                evicted.push_back(handleIndex);
            }
        }

        for (uint32_t handleIndex : evicted)
            m_failEvictItems.erase(handleIndex);
    }

    assert(minFreeMemory < m_maxMemory);
    while (m_usedMemory > m_maxMemory - minFreeMemory) {
        if (m_inMemoryItems.size() == 0) {
            spdlog::error("Cache is empty but actively hold items occupy more memory than allowed");
            break;
        }
        auto handleIndex = std::move(m_inMemoryItems.front());
        m_inMemoryItems.pop_front();

        std::weak_ptr<T> pItemWeak;
        size_t itemSize;
        {
            std::shared_ptr<T> pItem = m_lookUp[handleIndex].first;
            itemSize = pItem->sizeBytes();
            pItemWeak = pItem;
        }
        m_lookUp.erase(handleIndex);

        if (pItemWeak.expired())
            m_usedMemory -= itemSize;
        else
            m_failEvictItems[handleIndex] = { pItemWeak, itemSize };
    }
}

template <typename T>
inline CacheHandle<T> LRUCache<T>::Builder::registerCacheable(const T& item)
{
    uint32_t index = static_cast<uint32_t>(m_constructData.size());

    auto fileName = fmt::format("file{}.bin", index);
    m_constructData.push_back(fileName);

    // Serialize
    std::ofstream os { fileName, std::ios::binary };
    auto data = item.serialize();
    os.write(reinterpret_cast<const char*>(data.data()), data.size());

    return CacheHandle<T> { index };
}

template <typename T>
inline LRUCache<T> LRUCache<T>::Builder::build(size_t maxMemory)
{
    return LRUCache<T>(maxMemory, std::move(m_constructData));
}

}