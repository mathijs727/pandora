#include "stream/task_graph.h"
#include "stream/cache.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
#include <tuple>
#include <vector>

TEST(TaskGraph, SingleTask)
{
    constexpr size_t range = 128 * 1024;

    std::vector<int> output;
    output.resize(range, 0);

    tasking::TaskGraph g;
    auto task = g.addTask<int>([&](gsl::span<const int> numbers, std::pmr::memory_resource* pMemoryResource) {
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
    constexpr size_t range = 128 * 1024;

    std::vector<int> output;
    output.resize(range, 0);

    tasking::TaskGraph g;
    auto task4 = g.addTask<std::pair<int, int>>(
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const auto [initialNumber, number] : numbers)
                output[initialNumber] = number;
        });
    auto task3 = g.addTask<std::pair<int, int>>(
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            for (const auto [initialNumber, number] : numbers)
                g.enqueue(task4, { initialNumber, number * 2 });
        });
    auto task2 = g.addTask<std::pair<int, int>>(
        [&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
            std::pmr::vector<std::pair<int, int>> intermediate { pMemoryResource };
            std::transform(std::begin(numbers), std::end(numbers), std::back_inserter(intermediate),
                [](const std::pair<int, int>& pair) {
                    const auto [initialNumber, number] = pair;
                    return std::pair { initialNumber, number + 3 };
                });
            g.enqueue<std::pair<int, int>>(task3, intermediate);
        });
    auto task1 = g.addTask<int>([&](gsl::span<const int> numbers, std::pmr::memory_resource* pMemoryResource) {
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
    constexpr size_t range = 128 * 1024;

    std::vector<int> output;
    output.resize(range, 0);

    struct StaticData {
        int adder;
    };

    StaticData staticData;
    staticData.adder = 2;

    tasking::TaskGraph g;
    auto task = g.addTask<int, StaticData>(
        [&](gsl::span<const int> numbers, const StaticData* pStaticData, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                output[number] = number + pStaticData->adder;
        },
        []() {
            StaticData staticData;
            staticData.adder = 2;
            return staticData;
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], i + 2);
}

TEST(TaskGraph, CachedStaticData)
{
    constexpr size_t range = 128 * 1024;
    constexpr int numCacheItems = 100;
    constexpr int cacheSizeInItems = 8;

    struct StaticData {
        StaticData(StaticData&&) = default;
        StaticData(int v)
            : val(v)
        {
        }
        StaticData(gsl::span<const std::byte> data)
        {
            const StaticData* pData = reinterpret_cast<const StaticData*>(data.data());
            val = pData->val;
        }

        size_t sizeBytes()
        {
            return sizeof(StaticData);
        }
        std::vector<std::byte> serialize() const
        {
            auto start = reinterpret_cast<const std::byte*>(this);

            std::vector<std::byte> data;
            std::copy(start, start + sizeof(StaticData), std::back_inserter(data));
            return data;
        }

        int val;
        uint8_t dummy[1020];
    };

    std::array<stream::CacheHandle<StaticData>, numCacheItems> cacheItems;

    stream::LRUCache<StaticData>::Builder cacheBuilder;
    for (int i = 0; i < numCacheItems; i++) {
        auto handle = cacheBuilder.registerCacheable(StaticData(123));
        cacheItems[i] = handle;
    }
    auto cache = cacheBuilder.build(cacheSizeInItems * sizeof(StaticData));

    std::vector<int> output;
    output.resize(range, 0);

	struct StaticDataCollection {
        std::shared_ptr<StaticData> pStaticData;
	};

    tasking::TaskGraph g;
        auto task = g.addTask<int, StaticDataCollection>(
        [&](gsl::span<const int> numbers, const StaticDataCollection* pStaticDataCollection, std::pmr::memory_resource* pMemoryResource) {
            for (const int number : numbers)
                output[number] = number + pStaticDataCollection->pStaticData->val;
        },
        [&]() -> StaticDataCollection {
            return StaticDataCollection { cache.get(cacheItems[std::rand() % numCacheItems]) };
        });

    for (int i = 0; i < range; i++)
        g.enqueue(task, i);

    g.run();

    for (int i = 0; i < range; i++)
        ASSERT_EQ(output[i], i + 123);
}
