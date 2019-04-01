#pragma once
#include "task.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

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
        m_outputTask = &task2;
    }

protected:
    void execute(gsl::span<const InputT> inputData) override
    {
        /*tbb::blocked_range<int> range{ inputData.size() };
        tbb::parallel_for(range, [&]() {

        });*/
    }

private:
    Kernel m_cpuKernel;
    Task<OutputT>* m_outputTask = nullptr;
};

}