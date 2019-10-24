#include "stream/cache/evict_cache.h"
#include <cstddef>
#include <gsl/span>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

struct DummyData : public stream::Evictable {
    int v;
    std::vector<int> data;

    DummyData(int x, bool initiallyResident)
        : Evictable(initiallyResident)
        , v(x)
    {
        if (initiallyResident)
            _doMakeResident();
    }
    size_t sizeBytes() const override
    {
        return sizeof(DummyData) + data.size() * sizeof(int);
    }

    void serialize(stream::Serializer& serializer)
    {
    }

    void doEvict() override
    {
        data.clear();
        data.shrink_to_fit();
    }

    void doMakeResident(stream::Deserializer&) override
    {
        _doMakeResident();
    }

    void _doMakeResident()
    {
        data.resize(1000);
        data.shrink_to_fit();
        std::fill(std::begin(data), std::end(data), v);
    }
};

class DummyDeserializer : public stream::Deserializer {
    const void* map(const stream::Allocation&) final { return nullptr; };
    void unmap(const stream::Allocation&) final {};
};
class DummySerializer : public stream::Serializer {
    std::pair<stream::Allocation, void*> allocateAndMap(size_t) final { return { stream::Allocation {}, nullptr }; };
    void unmapPreviousAllocations() final {};

    std::unique_ptr<stream::Deserializer> createDeserializer() final
    {
        return std::make_unique<DummyDeserializer>();
    }
};

TEST(EvictLRUCache, MakesResident)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder { std::make_unique<DummySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pSharedOwner = cache.makeResident(&data[i]);
        ASSERT_TRUE(pSharedOwner->isResident());
    }
}

TEST(EvictLRUCache, ManualEvictFail)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++) {
        data.push_back(DummyData(i, i % 2 == 0));
    }

    stream::EvictLRUCache::Builder builder { std::make_unique<DummySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 2000;
    auto cache = builder.build(maxMemory);
    ASSERT_TRUE(cache.checkResidencyIsValid());

    for (auto& item : data)
        item.evict();

    ASSERT_FALSE(cache.checkResidencyIsValid());
}

TEST(EvictLRUCache, MemoryUsage)
{
    std::vector<DummyData> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyData(i, false));

    stream::EvictLRUCache::Builder builder { std::make_unique<DummySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 500;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pItem = cache.makeResident(&data[i]);
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

    stream::EvictLRUCache::Builder builder { std::make_unique<DummySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 250;
    auto cache = builder.build(maxMemory);

    std::vector<stream::CachedPtr<DummyData>> owningPtrs;
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
