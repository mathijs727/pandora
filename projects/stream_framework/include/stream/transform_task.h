#pragma once
#include "task.h"

namespace tasking {

template <typename InputT, typename OutputT>
class TransformTask : public Task<InputT> {
public:
    using Kernel = std::function<void(gsl::span<const InputT>, gsl::span<OutputT>)>;

    TransformTask(TaskPool& pool, Kernel&& cpuKernel)
        : m_cpuKernel(std::move(cpuKernel))
    {
    }
    TransformTask(const TransformTask&) = delete;
    TransformTask(TransformTask&&) = delete;

    void connect(Task<OutputT>& task2)
    {
        m_outputStream = &task2.getInputStreamReference();
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