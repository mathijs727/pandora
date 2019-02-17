#include "stream/common.h"

namespace stream {

void xxx()
{
    auto cpuKernel1 = [](const StreamRef<int> input, RegularOutput<float> output) {
        for (int i = 0; i < input.size(); i++)
            output.dataStream[i] = static_cast<float>(input[i]);
    };
    auto cpuKernel2 = [](const StreamRef<float> input, RegularOutput<int> output) {
        for (int i = 0; i < input.size(); i++)
            output.dataStream[i] = static_cast<int>(input[i]);
    };
    Task<int, RegularOutput<float>> task1 { cpuKernel1 };
    Task<float, RegularOutput<int>> task2 { cpuKernel2 };

    connectPort<0>(task1, task2);
}
}
