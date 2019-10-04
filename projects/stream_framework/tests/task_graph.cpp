#include "stream/task_graph.h"
#include <algorithm>
#include <gtest/gtest.h>
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
    auto task4 = g.addTask<std::pair<int, int>>([&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
        for (const auto [initialNumber, number] : numbers)
            output[initialNumber] = number;
    });
    auto task3 = g.addTask<std::pair<int, int>>([&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
        for (const auto [initialNumber, number] : numbers)
            g.enqueue(task4, { initialNumber, number * 2 });
    });
    auto task2 = g.addTask<std::pair<int, int>>([&](gsl::span<const std::pair<int, int>> numbers, std::pmr::memory_resource* pMemoryResource) {
        std::pmr::vector<std::pair<int, int>> intermediate { pMemoryResource };
        std::transform(std::begin(numbers), std::end(numbers), std::back_inserter(intermediate),
            [](const std::pair<int, int>& pair) {
                const auto [initialNumber, number] = pair;
                return std::pair { initialNumber, number + 3 };
            });
        g.enqueue<std::pair<int,int>>(task3, intermediate);
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
