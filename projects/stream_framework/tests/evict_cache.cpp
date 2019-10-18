#include "stream/evict_cache.h"
#include <cstddef>
#include <gsl/span>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

struct DummyData : public stream::Evictable {
    int v;
    std::vector<int> data;

    size_t sizeBytes() const override
    {
        return sizeof(DummyData) + data.size() * sizeof(int);
    }

    DummyData(int x, bool initiallyResident)
        : Evictable(initiallyResident)
        , v(x)
    {
        if (initiallyResident)
            doMakeResident();
    }

    void doEvict() override
    {
        data.clear();
        data.shrink_to_fit();
    }

    void doMakeResident() override
    {
        data.resize(1000);
        data.shrink_to_fit();
        std::fill(std::begin(data), std::end(data), v);
    }
};

TEST(EvictLRUCache, HandleToPointer)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder;
    std::vector<stream::CacheHandle<DummyData>> handles;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        handles.push_back(builder.registerCacheable(&data[i]));
    }

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.get(handles[i]);
        ASSERT_EQ(pItem.get(), &data[i]);
    }
}

TEST(EvictLRUCache, MakesResident)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder;
    std::vector<stream::CacheHandle<DummyData>> handles;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        handles.push_back(builder.registerCacheable(&data[i]));
    }

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.get(handles[i]);
        ASSERT_TRUE(pItem->isResident());
    }
}

TEST(EvictLRUCache, ManualMakeResidentFail)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++) {
        data.push_back(DummyData(i, i % 2 == 0));
    }

    stream::EvictLRUCache::Builder builder;
    std::vector<stream::CacheHandle<DummyData>> handles;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        handles.push_back(builder.registerCacheable(&data[i]));
    }

    const size_t maxMemory = data.size() * 2000;
    auto cache = builder.build(maxMemory);
    ASSERT_TRUE(cache.checkResidencyIsValid());

    data[1].makeResident();
    ASSERT_FALSE(cache.checkResidencyIsValid());
}

TEST(EvictLRUCache, MemoryUsage)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder;
    std::vector<stream::CacheHandle<DummyData>> handles;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        handles.push_back(builder.registerCacheable(&data[i]));
    }

    const size_t maxMemory = data.size() * 500;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.get(handles[i]);
        ASSERT_TRUE(pItem->isResident());
        ASSERT_GT(pItem->data.size(), 0);
        ASSERT_EQ(pItem.get(), &data[i]);
    }

    size_t memoryUsed = 0;
    for (const auto& item : data) {
        memoryUsed += item.sizeBytes();
    }
    ASSERT_LE(memoryUsed, maxMemory);
}

TEST(EvictLRUCache, EvictionHoldItems)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder;
    std::vector<stream::CacheHandle<DummyData>> handles;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        handles.push_back(builder.registerCacheable(&data[i]));
    }

    const size_t maxMemory = data.size() * 250;
    auto cache = builder.build(maxMemory);

    std::vector<stream::CachedPtr<DummyData>> owningPtrs;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.get(handles[i]);
        ASSERT_TRUE(pItem->isResident());
        owningPtrs.push_back(pItem);
    }

    ASSERT_EQ(data.size(), owningPtrs.size());
    for (const auto& pItem : owningPtrs) {
        ASSERT_TRUE(pItem->isResident());
    }
}
