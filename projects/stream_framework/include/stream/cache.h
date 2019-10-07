#pragma once
#include "templates.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <list>
#include <memory>
#include <memory_resource>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace stream {

namespace detail {
    template <typename T>
    struct DummyT {
    };
}

struct Handle {
    uint32_t index;
};

template <typename T>
using CacheHandle = StronglyTypedAlias<Handle, detail::DummyT<T>>;

template <typename T>
class LRUCache {
public:
    class Builder;
    T* get(CacheHandle<T> handle);

    size_t memoryUsage() const;

private:
    LRUCache(size_t maxMemory, std::vector<std::filesystem::path>&& constructData);

    T* load(CacheHandle<T> handle);
    void evict(size_t minFreeMemory);

private:
    using ItemsList = std::list<std::pair<uint32_t, std::unique_ptr<T>>>;
    ItemsList m_inMemoryItems;
    std::unordered_map<uint32_t, typename ItemsList::iterator> m_lookUp;

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
inline T* LRUCache<T>::get(CacheHandle<T> handle)
{
    // Look up
    if (auto mapIter = m_lookUp.find(handle.index); mapIter != std::end(m_lookUp)) {
        auto listIter = mapIter->second;

        // Remove from list and add to end
        m_inMemoryItems.splice(std::end(m_inMemoryItems), m_inMemoryItems, listIter);
        listIter = --std::end(m_inMemoryItems);

        mapIter->second = listIter;
        return listIter->second.get();
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
inline T* LRUCache<T>::load(CacheHandle<T> handle)
{
    std::vector<std::byte> data;
    std::ifstream is { m_constructData[handle.index], std::ios::binary };

    // Initialize cursor at end of file
    is.seekg(0, std::ios_base::end);
    std::streampos fileSize = is.tellg();
    data.resize(fileSize);

    is.seekg(0, std::ios_base::beg);
    is.read(reinterpret_cast<char*>(data.data()), fileSize);

    // Create instance from loaded bytes
    auto pItem = std::make_unique<T>(data);
    auto pItemNonOwning = pItem.get();
    m_usedMemory += pItem->sizeBytes();

    // Free memory if we went over the memory budget
    if (m_usedMemory > m_maxMemory)
        evict(pItem->sizeBytes());

    // Insert into cache
    m_inMemoryItems.emplace_back(std::pair { handle.index, std::move(pItem) });
    m_lookUp[handle.index] = --std::end(m_inMemoryItems);
    assert(m_inMemoryItems.size() <= m_constructData.size()); // Can never have more items in cache than there are registered.

    return pItemNonOwning;
}

template <typename T>
inline void LRUCache<T>::evict(size_t minFreeMemory)
{
    assert(m_maxMemory > minFreeMemory);
    while (m_usedMemory > m_maxMemory - minFreeMemory) {
        auto [handleIndex, pItem] = std::move(m_inMemoryItems.front());
        m_inMemoryItems.pop_front();

        m_usedMemory -= pItem->sizeBytes();
        m_lookUp.erase(handleIndex);
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