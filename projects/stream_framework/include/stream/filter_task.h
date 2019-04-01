#pragma once
#include "task.h"

namespace tasking {

template <typename InputT, typename OutputT>
class FilteredTransformTask : public Task<InputT> {
public:
    using Kernel = std::function<void(gsl::span<const InputT>, gsl::span<bool>, gsl::span<OutputT>)>;

    FilteredTransformTask(TaskPool& pool, Kernel&& cpuKernel)
        : m_cpuKernel(std::move(cpuKernel))
    {
    }
    FilteredTransformTask(const FilteredTransformTask&) = delete;
    FilteredTransformTask(FilteredTransformTask&&) = delete;

    void connect(Task<OutputT>& task2)
    {
        m_outputStream = &task2.m_inputData;
    }

protected:
    void execute() override
    {
    }

private:
    Kernel m_cpuKernel;
    DataStream<OutputT>* m_outputStream = nullptr;
};

}