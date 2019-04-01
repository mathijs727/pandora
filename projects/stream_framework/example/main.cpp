#include "stream/task_pool.h"
#include "stream/transform_task.h"
#include <future>
#include <iostream>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>
#include <tbb/tbb.h>
#include <thread>

void testGraphBuilder();

static constexpr int stepSize = 1024;
static constexpr int streamSize = 8 * 1024 * 1024;

int main()
{
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);

    //datastreamTestSingleThreaded();
    //dataStreamTestMultiThreadPush();
    //dataStreamTestProducerConsumer();
}

void testGraphBuilder()
{
    using namespace tasking;

    auto cpuKernel1 = [](gsl::span<const int> input, gsl::span<float> output) {
        for (int i = 0; i < input.size(); i++)
            output[i] = static_cast<float>(input[i]);
    };
    auto cpuKernel2 = [](gsl::span<const float> input, gsl::span<int> output) {
        for (int i = 0; i < input.size(); i++)
            output[i] = static_cast<int>(input[i]);
    };

    TaskPool p {};
    auto t1 = TransformTask<int, float>(p, cpuKernel1);
    auto t2 = TransformTask<float, int>(p, cpuKernel2);

    t1.connect(t2);
}
