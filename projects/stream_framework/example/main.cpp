#include "stream/task_pool.h"
#include "stream/transform_task.h"
#include <iostream>

void testGraphBuilder();

int main()
{
    //testGraphBuilder();
    using namespace tasking;

    int streamSize = 1024 * 1024 * 1024;
    DataStream<int> stream { static_cast<size_t>(streamSize / 8) };
    for (int i = 0; i < streamSize; i++) {
        stream.push(i);
    }

    int sum = 0;
    while (!stream.isEmpty()) {
        const auto dataBlock = stream.getBlock();
        for (int v : dataBlock.getData()) {
            if ((v % 2) == 0)
                sum += v;
            else
                sum -= v;
        }
    }
    std::cout << "Sum: " << sum << std::endl;
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

