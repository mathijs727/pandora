#pragma once
#include "task.h"
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

namespace tasking {

template <typename InputT, typename OutputT>
class TransformTask : public Task<InputT> {
public:
    using Kernel = std::function<void(gsl::span<const InputT>, gsl::span<OutputT>)>;

    TransformTask(TaskPool& pool, Kernel&& cpuKernel)
        : Task<InputT>(pool, 1)
        , m_cpuKernel(std::move(cpuKernel))
    {
    }
    TransformTask(const TransformTask&) = delete;
    TransformTask(TransformTask&&) = delete;

    void connect(Task<OutputT>* task, int streamID = 0)
    {
        m_outputTask = task;
        m_outputStreamID = streamID;
        assert(m_outputStreamID < m_outputTask->numInputStreams());
    }

protected:
    void execute(DataStream<InputT>& dataStream) override
    {
        for (auto dataBlock: dataStream.consume())
        {
            std::vector<OutputT> output(dataBlock.size());
            m_cpuKernel(dataBlock, output);

            if (m_outputTask) {
                m_outputTask->push(m_outputStreamID, output);
            }
        }
        /*tbb::blocked_range<int> range{ inputData.size() };
        tbb::parallel_for(range, [&]() {

        });*/
    }

private:
    Kernel m_cpuKernel;

    Task<OutputT>* m_outputTask = nullptr;
    int m_outputStreamID = -1;
};

}
