#pragma once
#include <array>
#include <functional>
#include <gsl/span>

namespace tasking {

template <typename T>
using StreamRef = gsl::span<T>;

template <typename T>
struct RegularOutput {
    using Type = T;

    StreamRef<T> dataStream;
};

template <typename T>
struct FilteredOutput {
    using Type = T;

    StreamRef<T> dataStream;
    StreamRef<bool> validStream;
};

template <typename T>
struct ScatterOutput {
    StreamRef<uint32_t> destinationStream;
};

template <typename Input, typename... Outputs>
class Task {
public:
    using Kernel = std::function<void(const StreamRef<Input>, Outputs...)>;

    Task(Kernel&& cpuKernel)
        : m_cpuKernel(std::move(cpuKernel))
    {
    }
    Task(Task&&) = default;

protected:
    // Called by scheduler
    void executeCPU(const StreamRef<Input> input, Outputs... outputs)
    {
        m_cpuKernel(input, outputs...);
    }

private:
    Kernel m_cpuKernel;
};

template <int PortID, typename Task1, typename Task2>
void connectPort(const Task1& t1, const Task2& t2)
{
}

}