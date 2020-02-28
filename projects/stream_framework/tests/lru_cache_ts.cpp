#include "stream/cache/lru_cache_ts.h"
#include "stream/serialize/in_memory_serializer.h"
#include <cstddef>
#include <cstring>
#include <gsl/span>
#include <gtest/gtest.h>
#include <random>
#include <spdlog/spdlog.h>
#include <tbb/parallel_for.h>
#include <thread>
#include <vector>

struct DummyDataTS : public tasking::Evictable {
    int value;
    tasking::Allocation alloc;

    DummyDataTS(int v)
        : Evictable(true)
        , value(v)
    {
    }
    size_t sizeBytes() const override
    {
        size_t size = sizeof(DummyDataTS);
        if (isResident())
            size += 1000;
        return size;
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
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        deserializer.unmap(pInt);
    }
};

TEST(LRUCacheTS, MakesResident)
{
    std::vector<DummyDataTS> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyDataTS(i));

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i], false);
    }

    for (const DummyDataTS& item : data)
        ASSERT_FALSE(item.isResident());

    const size_t maxMemory = data.size() * 750;
    auto cache = builder.build(maxMemory);
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        auto pSharedOwner = cache.makeResident(&data[i]);
        ASSERT_TRUE(pSharedOwner->isResident());
    }
}

TEST(LRUCacheTS, RegisterAndEvict)
{
    std::vector<DummyDataTS> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyDataTS(i));

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
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

/*TEST(LRUCacheTS, ManualEvictFail)
{
    std::vector<DummyDataTS> data;
    for (int i = 0; i < 50; i++) {
        data.push_back(DummyDataTS(i, i % 2 == 0));
    }

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::DummySerializer>() };
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

TEST(LRUCacheTS, MemoryUsage)
{
    std::vector<DummyDataTS> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyDataTS(i));

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
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

TEST(LRUCacheTS, EvictionHoldItems)
{
    std::vector<DummyDataTS> data;
    for (int i = 0; i < 50; i++)
        data.push_back(DummyDataTS(i));

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (int i = 0; i < static_cast<int>(data.size()); i++) {
        builder.registerCacheable(&data[i]);
    }

    const size_t maxMemory = data.size() * 250;
    auto cache = builder.build(maxMemory);

    std::vector<tasking::CachedPtr<DummyDataTS>> owningPtrs;
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

TEST(LRUCacheTS, MultithreadedSum)
{
    const int numItems = 100000;

    int refSum = 0;
    std::vector<DummyDataTS> data;
    for (int i = 0; i < numItems; i++) {
        const int v = std::rand() % numItems;
        data.push_back(DummyDataTS(v));
        refSum += v;
    }

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (auto& d : data)
        builder.registerCacheable(&d, true);

    const size_t maxMemory = data.size() * 750; // Restrict max memory to force eviction ("fake" size = 40+1000 bytes)
    auto cache = builder.build(maxMemory);

    std::atomic_int sum = 0;
    tbb::parallel_for(tbb::blocked_range<int>(0, numItems),
        [&](tbb::blocked_range<int> localRange) {
            for (int i = std::begin(localRange); i != std::end(localRange); i++) {
                DummyDataTS* pDataItem = &data[i];
                auto pOwner = cache.makeResident(pDataItem);
                sum.fetch_add(pOwner->value);
            }
        });

    ASSERT_EQ(refSum, sum);
}

TEST(LRUCacheTS, MultithreadedAccess)
{
    const int numItems = 1000;

    int refSum = 0;
    std::vector<DummyDataTS> data;
    for (int i = 0; i < numItems; i++) {
        const int v = std::rand() % numItems;
        data.push_back(DummyDataTS(v));
        refSum += v;
    }

    tasking::LRUCacheTS::Builder builder { std::make_unique<tasking::InMemorySerializer>() };
    for (auto& d : data)
        builder.registerCacheable(&d, true);

    const size_t maxMemory = data.size() * 750; // Restrict max memory to force eviction ("fake" size = 40+1000 bytes)
    auto cache = builder.build(maxMemory);

    static constexpr size_t numThreads = 8;
    std::vector<int> answers;
    answers.resize(numThreads);
    std::vector<std::thread> threads;
    for (size_t t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {
            int sum = 0;
            for (int i = 0; i != numItems; i++) {
                DummyDataTS* pDataItem = &data[i];
                auto pOwner = cache.makeResident(pDataItem);
                sum += pOwner->value;
            }
            answers[t] = sum;
        });
    }

    for (auto& thread : threads)
        thread.join();

    for (int answer : answers)
        ASSERT_EQ(answer, refSum);
}
