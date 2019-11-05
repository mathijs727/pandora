#include "stream/task_graph.h"
#include "stream/cache/lru_cache.h"
#include "stream/serialize/dummy_serializer.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <tuple>
#include <vector>

TEST(TaskGraph, SingleTask)
{
    constexpr size_t range = 1024;

    std::vector<int> output;
    output.resize(range, 0);

    tasking::TaskGraph g;
    auto task = g.addTask<int>(
        "task",
        [&](gsl::span<const int> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                output[number]++;
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], 1);
}

TEST(TaskGraph, TaskChain)
{
    constexpr size_t range = 1024;

    std::vector<int> output;
    output.resize(range, 0);

    tasking::TaskGraph g;
    auto task4 = g.addTask<std::pair<int, int>>(
        "task4",
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const auto [initialNumber, number] : numbers)
                output[initialNumber] = number;
        });
    auto task3 = g.addTask<std::pair<int, int>>(
        "task3",
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const auto [initialNumber, number] : numbers)
                g.enqueue(task4, { initialNumber, number * 2 });
        });
    auto task2 = g.addTask<std::pair<int, int>>(
        "task2",
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            std::pmr::vector<std::pair<int, int>> intermediate { pMemoryResource };
            std::transform(std::begin(numbers), std::end(numbers), std::back_inserter(intermediate),
                [](const std::pair<int, int>& pair) {
                    const auto [initialNumber, number] = pair;
                    return std::pair { initialNumber, number + 3 };
                });
            g.enqueue<std::pair<int, int>>(task3, intermediate);
        });
    auto task1 = g.addTask<int>(
        "task1",
        [&](gsl::span<const int> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                g.enqueue(task2, { number, number * 3 });
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task1, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], ((i * 3) + 3) * 2);
}

TEST(TaskGraph, StaticData)
{
    constexpr size_t range = 1024;

    std::vector<int> output;
    output.resize(range, 0);

    struct StaticData {
        int adder;
    };

    StaticData staticData;
    staticData.adder = 2;

    tasking::TaskGraph g;
    auto task = g.addTask<int, StaticData>(
        "task",
        []() {
            StaticData staticData;
            staticData.adder = 2;
            return staticData;
        },
        [&](gsl::span<const int> numbers, const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                output[number] = number + pStaticData->adder;
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], i + 2);
}

TEST(TaskGraph, CachedStaticData)
{
    constexpr size_t range = 1024;
    constexpr int numCacheItems = 50;
    constexpr int cacheSizeInItems = 8;

    struct StaticData : public tasking::Evictable {
        int v;
        std::vector<int> data;

        StaticData(int x, bool initiallyResident)
            : Evictable(initiallyResident)
            , v(x)
        {
            if (initiallyResident)
                _doMakeResident();
        }
        size_t sizeBytes() const override
        {
            return sizeof(StaticData) + data.capacity() * sizeof(int);
        }

        void serialize(tasking::Serializer& serializer)
        {
        }

        void doEvict() override
        {
            data.clear();
            data.shrink_to_fit();
        }

        void doMakeResident(tasking::Deserializer&) override
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

    std::vector<StaticData> cacheItems;
    for (int i = 0; i < numCacheItems; i++) {
        cacheItems.push_back(StaticData(123, false));
    }

    tasking::LRUCache::Builder cacheBuilder { std::make_unique<tasking::DummySerializer>() };
    for (auto& cacheItem : cacheItems)
        cacheBuilder.registerCacheable(&cacheItem);

    auto cache = cacheBuilder.build(cacheSizeInItems * sizeof(StaticData));

    std::vector<int> output;
    output.resize(range, 0);

    struct StaticDataCollection {
        tasking::CachedPtr<StaticData> pStaticData;
    };

    tasking::TaskGraph g;
    auto task = g.addTask<int, StaticDataCollection>(
        "task",
        [&]() -> StaticDataCollection {
            return StaticDataCollection { cache.makeResident(&cacheItems[std::rand() % numCacheItems]) };
        },
        [&](gsl::span<const int> numbers, const StaticDataCollection* pStaticDataCollection, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                output[number] = number + pStaticDataCollection->pStaticData->v;
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], 123 + i);
}
