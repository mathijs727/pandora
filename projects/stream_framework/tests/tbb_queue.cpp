#include "stream/queue/tbb_queue.h"
#include <atomic>
#include <gtest/gtest.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/task_group.h>
#include <vector>

struct Data {
    int i;
    int __padding[8];
};

TEST(TBBQueue, SingleThreaded)
{
    tasking::TBBQueue<Data> queue {};

    constexpr int numItems = 10000;
    for (int i = 0; i < numItems; i++)
        queue.push(Data { i });

    std::vector<bool> visited;
    visited.resize(numItems);
    std::fill(std::begin(visited), std::end(visited), false);

    Data d;
    while (queue.try_pop(d)) {
        visited[d.i] = true;
    }

    for (int i = 0; i < numItems; i++)
        ASSERT_TRUE(visited[i]);
}

TEST(TBBQueue, MultiProducerSingleConsumer)
{
    constexpr int numItems = 20000;
    std::vector<bool> visited;
    visited.resize(numItems);
    std::fill(std::begin(visited), std::end(visited), false);

    tasking::TBBQueue<Data> queue {};

    constexpr int blockSize = 5000;
    tbb::task_group tg;
    std::atomic_int activeProducers { 0 };
    for (int i = 0; i < numItems; i += blockSize) {
        activeProducers.fetch_add(1);
        tg.run([&, i]() {
            for (int j = i; j < std::min(i + blockSize, numItems); j++) {
                queue.push(Data { j });
            }
            activeProducers.fetch_sub(1);
        });
    }

    auto pop = [&]() {
        Data d;
        while (queue.try_pop(d)) {
            visited[d.i] = true;
        }
    };

    while (activeProducers.load() > 0) {
        pop();
    }
    pop();

    tg.wait();

    for (int i = 0; i < numItems; i++)
        ASSERT_TRUE(visited[i]);
}
