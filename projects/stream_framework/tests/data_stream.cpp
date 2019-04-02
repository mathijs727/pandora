#include "stream/task_pool.h"
#include "stream/transform_task.h"
#include <future>
#include <gtest/gtest.h>
#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/tbb.h>
#include <thread>
#include <memory>
#include <atomic>

void testGraphBuilder();

static constexpr int stepSize = 1024;
static constexpr int streamSize = 8 * 1024 * 1024;

TEST(DataStream, SingleThreaded)
{
    tasking::DataStream<int> stream;

    for (int i = 0; i < streamSize; i += stepSize) {
        std::array<int, stepSize> data;
        for (int j = 0, k = i; j < stepSize; j++, k++)
            data[j] = k;
        stream.push(data);
    }

    int sum = 0;
    for (auto data : stream.consume()) {
        for (int v : data) {
            if ((v % 2) == 0)
                sum += v;
            else
                sum -= v;
        }
    }

    ASSERT_EQ(sum, -4194304);
}

TEST(DataStream, MultiThreadedPush)
{
    tasking::DataStream<int> stream;
    
    tbb::blocked_range<int> range = tbb::blocked_range<int>(0, streamSize, stepSize);
    tbb::parallel_for(range, [&](tbb::blocked_range<int> localRange) {
        std::vector<int> data(localRange.size());
        for (int i = 0, j = localRange.begin(); j < localRange.end(); i++, j++)
            data[i] = j;
        stream.push(std::move(data));
    });

    int sum = 0;
    for (auto data : stream.consume()) {
        for (int v : data) {
            if ((v % 2) == 0)
                sum += v;
            else
                sum -= v;
        }
    }

    ASSERT_EQ(sum, -4194304);
}

TEST(DataStream, ProducerConsumer)
{
    tasking::DataStream<int> stream;
    std::thread producer([&]() {
        for (int i = 0; i < streamSize; i += stepSize) {
            std::array<int, stepSize> data;
            for (int j = 0, k = i; j < stepSize; j++, k++)
                data[j] = k;
            stream.push(data);
        }
    });

    std::promise<int> sumPromise;
    std::thread consumer([&]() {
        int sum = 0;
        while (sum != -4194304) {
            for (auto data : stream.consume()) {
                for (int v : data) {
                    if ((v % 2) == 0)
                        sum += v;
                    else
                        sum -= v;
                }
            }
        }
        sumPromise.set_value(sum);
    });

    producer.join();
    consumer.join();
    int sum = sumPromise.get_future().get();

    ASSERT_EQ(sum, -4194304);
}
