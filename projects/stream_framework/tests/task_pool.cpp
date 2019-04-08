#include "stream/task_pool.h"
#include "stream/source_task.h"
#include "stream/transform_task.h"
#include <gtest/gtest.h>

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

    hpx::future<void> produce() final
    {
        int numItems = static_cast<int>(itemsToProduceUnsafe());

        std::vector<int> data(numItems);
        for (int i = 0; i < numItems; i++) {
            data[i] = m_current++;
            //spdlog::info("Gen: {}", data[i]);
        }
        m_consumer->push(0, std::move(data));
        co_return; // Important! Creates a coroutine.
    }

    size_t itemsToProduceUnsafe() const final
    {
        return std::min(m_end - m_current, m_itemsPerBatch);
    }

private:
    const int m_start, m_end, m_itemsPerBatch;
    int m_current;

    std::atomic_int m_currentlyInPool;

    gsl::not_null<tasking::Task<int>*> m_consumer;
};

TEST(TaskPool, Transform)
{
    using namespace tasking;

    constexpr int problemSize = 1024;
    constexpr int stepSize = 256;

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

        sum += localSum;
    };

    TaskPool p {};
    TransformTask intToFloat(p, cpuKernel1, defaultDataLocalityEstimate);
    RangeSource source = RangeSource(p, &intToFloat, 0, problemSize, stepSize);
    p.run();

    static_assert(problemSize % 2 == 0);
    ASSERT_EQ(sum, -problemSize / 2);
}
