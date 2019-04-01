#include "stream/task_pool.h"
#include "stream/transform_task.h"
#include <future>
#include <tbb/tbb.h>
#include <thread>
#include <gtest/gtest.h>

void testGraphBuilder();

static constexpr int stepSize = 1024;
static constexpr int streamSize = 8 * 1024 * 1024;

TEST(DataStream, SingleThreaded)
{
    tasking::DataStream<int> stream{ static_cast<size_t>(streamSize / 8) };
    for (int i = 0; i < streamSize; i += stepSize) {
        auto write = stream.reserveRangeForWriting(stepSize);
        for (int j = 0, k = i; j < stepSize; j++, k++)
            write.data[j] = k;
    }

    int sum = 0;
    while (auto dataChunkOpt = stream.popChunk()) {
        for (int v : dataChunkOpt->getData()) {
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
    tasking::DataStream<int> stream{ static_cast<size_t>(streamSize / 8) };

    tbb::blocked_range<int> range = tbb::blocked_range<int>(0, streamSize, stepSize);
    tbb::parallel_for(range, [&](tbb::blocked_range<int> localRange) {
        auto write = stream.reserveRangeForWriting(localRange.size());
        for (int i = 0, j = localRange.begin(); i < localRange.size(); i++, j++)
            write.data[i] = j;
    });

    int sum = 0;
    while (auto dataChunkOpt = stream.popChunk()) {
        for (int v : dataChunkOpt->getData()) {
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
    tasking::DataStream<int> stream{ static_cast<size_t>(streamSize / 8) };
    std::thread producer([&]() {
        for (int i = 0; i < streamSize; i += stepSize) {
            auto write = stream.reserveRangeForWriting(stepSize);
            for (int j = 0, k = i; j < stepSize; j++, k++)
                write.data[j] = k;
        }
    });

    std::promise<int> sumPromise;
    std::thread consumer([&]() {
        int sum = 0;
        while (sum != -4194304) {
            while (auto dataChunkOpt = stream.popChunk()) {
                for (int v : dataChunkOpt->getData()) {
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
