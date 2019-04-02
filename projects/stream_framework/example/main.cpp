#include "stream/source_task.h"
#include "stream/task_pool.h"
#include "stream/transform_task.h"
#include <iostream>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>

void testGraphBuilder();

static constexpr int stepSize = 1024;
static constexpr int streamSize = 8 * 1024 * 1024;

int main()
{
    auto vsLogger = spdlog::create<spdlog::sinks::msvc_sink_mt>("vs_logger");
    spdlog::set_default_logger(vsLogger);

    testGraphBuilder();
}

class RangeSource : public tasking::SourceTask {
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
            spdlog::info("Gen: {}", data[i]);
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

    gsl::not_null<tasking::Task<int>*> m_consumer;
};

void testGraphBuilder()
{
    using namespace tasking;

    auto cpuKernel1 = [](gsl::span<const int> input, gsl::span<float> output) {
        for (int i = 0; i < input.size(); i++) {
            output[i] = static_cast<float>(input[i]) + 0.1f;
            spdlog::info("{} => {}", input[i], output[i]);
        }
    };
    auto cpuKernel2 = [](gsl::span<const float> input, gsl::span<int> output) {
        for (int i = 0; i < input.size(); i++) {
            output[i] = static_cast<int>(input[i]);
            spdlog::info("{} => {}", input[i], output[i]);
        }
    };

    TaskPool p {};
    TransformTask<int, float> intToFloat(p, cpuKernel1);
    TransformTask<float, int> floatToInt(p, cpuKernel2);
    intToFloat.connect(&floatToInt);

    RangeSource source { p, &intToFloat, 0, 6, 2 };

    p.run();
}
