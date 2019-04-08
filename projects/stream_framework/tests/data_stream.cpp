#include <atomic>
#include <gtest/gtest.h>
#include <hpx/parallel/algorithm.hpp>
#include "stream/data_stream.h"
#include <thread>

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

    static_assert(streamSize % 2 == 0);
    ASSERT_EQ(sum, -streamSize / 2);
}

TEST(DataStream, MultiThreadedPush)
{
    tasking::DataStream<int> stream;
    static_assert(streamSize % stepSize == 0);
    hpx::parallel::for_loop(0, streamSize / stepSize, [&](int block) {
        int blockStart = block * stepSize;

        std::vector<int> data(stepSize);
        for (int i = 0; i < stepSize; i++)
            data[i] = blockStart + i;
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

    static_assert(streamSize % 2 == 0);
    ASSERT_EQ(sum, -streamSize / 2);
}

TEST(DataStream, ProducerConsumer)
{
    static_assert(streamSize % 2 == 0);

    tasking::DataStream<int> stream;
    auto producer = hpx::async([&]() {
        for (int i = 0; i < streamSize; i += stepSize) {
            std::array<int, stepSize> data;
            for (int j = 0, k = i; j < stepSize; j++, k++)
                data[j] = k;
            stream.push(data);
        }
    });

    auto consumer = hpx::async([&]() {
        int sum = 0;
        while (sum != -streamSize / 2) {
            for (auto data : stream.consume()) {
                for (int v : data) {
                    if ((v % 2) == 0)
                        sum += v;
                    else
                        sum -= v;
                }
            }
        }
        return sum;
    });

    producer.wait();
    int sum = consumer.get();

    ASSERT_EQ(sum, -streamSize / 2);
}

