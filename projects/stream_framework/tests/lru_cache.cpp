#include "stream/cache/lru_cache.h"
#include "stream/serialize/in_memory_serializer.h"
#include <cstddef>
#include <gsl/span>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

struct DummyData : public tasking::Evictable {
    int value;
    tasking::Allocation alloc;

    DummyData(int v)
        : Evictable(true)
        , value(v)
    {
    }
    size_t sizeBytes() const override
    {
        return sizeof(DummyData);
    }

    void serialize(tasking::Serializer& serializer)
    {
        auto [allocation, pInt] = serializer.allocateAndMap(sizeof(int));
        std::memcpy(pInt, &value, sizeof(int));
        serializer.unmapPreviousAllocations();

		alloc = allocation;
    }

    void doEvict() override
    {
        value = -1;
    }

    void doMakeResident(tasking::Deserializer& deserializer) override
    {
        const int* pInt = reinterpret_cast<const int*>(deserializer.map(alloc));
        value = *pInt;
        deserializer.unmap(alloc);
    }
};

TEST(LRUCache, MakesResident)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i));

    tasking::LRUCache::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i], false);
    }

	for (const DummyData& item : data)
        ASSERT_FALSE(item.isResident());

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pSharedOwner = cache.makeResident(&data[i]);
        ASSERT_TRUE(pSharedOwner->isResident());
    }
}

TEST(LRUCache, RegisterAndEvict)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i));

    tasking::LRUCache::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i], true);
    }

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pSharedOwner = cache.makeResident(&data[i]);
        ASSERT_TRUE(pSharedOwner->isResident());
    }
}

/*TEST(LRUCache, ManualEvictFail)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++) {
        data.push_back(DummyData(i, i % 2 == 0));
    }

    tasking::LRUCache::Builder builder { std::make_unique<tasking::DummySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 2000;
    auto cache = builder.build(maxMemory);
    ASSERT_TRUE(cache.checkResidencyIsValid());

    for (auto& item : data)
        item.evict();

    ASSERT_FALSE(cache.checkResidencyIsValid());
}*/

TEST(LRUCache, MemoryUsage)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i));

    tasking::LRUCache::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 500;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.makeResident(&data[i]);
        ASSERT_TRUE(pItem->isResident());
        ASSERT_EQ(pItem->value, i);
    }

    size_t memoryUsed = 0;
    for (const auto& item : data) {
        memoryUsed += item.sizeBytes();
    }
    ASSERT_LE(memoryUsed, maxMemory);
}

TEST(LRUCache, EvictionHoldItems)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i));

    tasking::LRUCache::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 250;
    auto cache = builder.build(maxMemory);

    std::vector<tasking::CachedPtr<DummyData>> owningPtrs;
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.makeResident(&data[i]);
        ASSERT_TRUE(pItem->isResident());
        owningPtrs.push_back(pItem);
    }

    ASSERT_EQ(data.size(), owningPtrs.size());
    for (const auto& pItem : owningPtrs) {
        ASSERT_TRUE(pItem->isResident());
    }
}
