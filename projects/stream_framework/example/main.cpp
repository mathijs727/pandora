//#include "stream/resource_cache.h"
//#include "stream/source_task.h"
#include "stream/task_pool.h"
//#include "stream/transform_task.h"
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>
#include <hpx/hpx_init.hpp>
#include <hpx/hpx_main.hpp>

void testTaskPool();

int main()
{
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);

    //testTaskPool();

    return 0;
}

/*class RangeSource : public tasking::SourceTask {
public:
    RangeSource(tasking::TaskPool& taskPool, gsl::not_null<tasking::Task<int>*> consumer, int start, int end, int itemsPerBatch)
        : SourceTask(taskPool)
        , m_start(start)
        , m_end(end)
        , m_itemsPerBatch(itemsPerBatch)
        , m_current(start)
        , m_consumer(consumer)
    {
    }

    void produce() final
    {
        int numItems = static_cast<int>(itemsToProduce());

        std::vector<int> data(numItems);
        for (int i = 0; i < numItems; i++) {
            data[i] = m_current++;
            //spdlog::info("Gen: {}", data[i]);
        }
        m_consumer->push(0, std::move(data));
    }

    size_t itemsToProduce() const final
    {
        return std::min(m_end - m_current, m_itemsPerBatch);
    }

private:
    const int m_start, m_end, m_itemsPerBatch;
    int m_current;

    std::atomic_int m_currentlyInPool;

    gsl::not_null<tasking::Task<int>*> m_consumer;
};

void testTaskPool()
{
    using namespace tasking;

    constexpr int problemSize = 1024 * 1024;
    constexpr int stepSize = 1024;

    std::atomic_int sum = 0;
    auto cpuKernel1 = [&](gsl::span<const int> input, gsl::span<float> output) {
        int localSum = 0;

        for (int i = 0; i < input.size(); i++) {
            output[i] = static_cast<float>(input[i]);

            int v = input[i];
            if (v % 2 == 0)
                localSum += v;
            else
                localSum -= v;
        }

        // Emulate doing 1 microsecond of work for each item.
        //std::this_thread::sleep_for(std::chrono::microseconds(stepSize));

        //sum += localSum;
    };
    auto cpuKernel2 = [](gsl::span<const float> input, gsl::span<int> output) {
        for (int i = 0; i < input.size(); i++) {
            output[i] = static_cast<int>(input[i]);
        }
    };

    TaskPool p {};
    TransformTask intToFloat(p, cpuKernel1, defaultDataLocalityEstimate);
    //TransformTask<float, int> floatToInt(p, cpuKernel2);
    //intToFloat.connect(&floatToInt);

    RangeSource source = RangeSource(p, &intToFloat, 0, problemSize, stepSize);
    p.run();

    static_assert(problemSize % 2 == 0);
    if (sum == -problemSize / 2)
        spdlog::info("CORRECT ANSWER");
    else
        spdlog::critical("INCORRECT ANSWER");
}*/
