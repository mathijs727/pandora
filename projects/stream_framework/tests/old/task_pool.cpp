#include "stream/task_pool.h"
#include "stream/task_executor.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

static tasking::SourceTask<int>* createIntRangeSource(tasking::TaskPool& taskPool, int start, int end, int batchSize)
{
    struct State {
        int current;
    };
    auto pState = std::make_shared<State>();
    pState->current = start;
    return taskPool.createSourceTask<int>(
        [=](std::span<int> output) mutable {
            int i = pState->current;
            std::generate(std::begin(output), std::end(output), [&]() { return i++; });
            pState->current = i;
        },
        [=]() -> size_t {
            return std::min(batchSize, std::max(0, end - pState->current));
        });
}

TEST(TaskPool, Transform)
{
    using namespace tasking;

    constexpr int problemSize = 1024;
    constexpr int batchSize = 64;

    std::atomic_int sum = 0;
    auto cpuKernel1 = [&](std::span<const int> input, std::span<float> output) {
        int localSum = 0;

		spdlog::info("int to float {}", input.size());
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

    TaskPool pool {};
    TransformTask<int, float>* pIntToFloat = pool.createTransformTask<int, float>(cpuKernel1);
    SourceTask<int>* pSource = createIntRangeSource(pool, 0, problemSize, batchSize);
    pSource->connect(pIntToFloat);

    TaskExecutor executor { pool };
    executor.run();

    static_assert(problemSize % 2 == 0);
    ASSERT_EQ(sum, -problemSize / 2);
}
