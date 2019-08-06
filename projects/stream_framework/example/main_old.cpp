#include "stream/data_stream.h"
#include <chrono>
#include <hpx/hpx_init.hpp>
#include <hpx/hpx_main.hpp>
#include <hpx/parallel/algorithm.hpp>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_queue.h>

void producerConsumerDataStream();
void producerConsumerTBB();

int main()
{
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);

    using clock_t = std::chrono::high_resolution_clock;
    using duration_t = std::chrono::duration<double>;
    {
        auto start = clock_t::now();
        producerConsumerDataStream();
        auto end = clock_t::now();
        duration_t duration = end - start;
        spdlog::info("DataStream timing {}s", duration.count());
    }

    {
        auto start = clock_t::now();
        producerConsumerTBB();
        auto end = clock_t::now();
        duration_t duration = end - start;
        spdlog::info("TBB timing {}s", duration.count());
	}

    return 0;
}

static constexpr int stepSize = 1024;
static constexpr int streamSize = 8 * 1024 * 1024;

void producerConsumerDataStream()
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

    assert(sum == -streamSize / 2);
}

void producerConsumerTBB()
{
    static_assert(streamSize % 2 == 0);

    tbb::concurrent_queue<int> stream;
    auto producer = hpx::async([&]() {
        for (int i = 0; i < streamSize; i++) {
			stream.push(i);
        }
    });

    auto consumer = hpx::async([&]() {
        int sum = 0;
        while (sum != -streamSize / 2) {
            int v;
            while (!stream.try_pop(v))
                ;

            if ((v % 2) == 0)
                sum += v;
            else
                sum -= v;
        }
        return sum;
    });

    producer.wait();
    int sum = consumer.get();

    assert(sum, -streamSize / 2);
}
