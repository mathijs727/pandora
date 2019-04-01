#pragma once
#include "task.h"

namespace tasking {

// Scatter input to 0 or 1 outputs. Outputs may be of different types and there may be multiple outputs with the same type.
template <typename InputT, typename OutputT>
class MultiTypeScatterTransformTask;

template <typename InputT, typename... OutputTs>
class MultiTypeScatterTransformTask<InputT, std::tuple<OutputTs...>> : public Task<InputT> {
public:
    using Kernel = std::function<void(gsl::span<const InputT>, gsl::span<int32_t>, gsl::span<OutputTs>...)>;

    MultiTypeScatterTransformTask(TaskPool& pool, Kernel&& cpuKernel)
        : m_cpuKernel(std::move(cpuKernel))
    {
    }
    MultiTypeScatterTransformTask(const MultiTypeScatterTransformTask&) = delete;
    MultiTypeScatterTransformTask(MultiTypeScatterTransformTask&&) = delete;

protected:
    void execute() override
    {
    }

private:
    Kernel m_cpuKernel;
};

}