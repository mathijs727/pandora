#pragma once
#include "stream/task.h"
#include <functional>

namespace tasking {

template <typename Input, typename Output>
class TransformTask : public Task<Input> {
public:
    void connect(Task<Output>* pTask);

    void execute() final;

private:
    friend class TaskPool;
    using Kernel = std::function<void(std::span<const Input>, std::span<Output>)>;
    TransformTask(Kernel&& kernel);

private:
    Kernel m_kernel;
    Task<Output>* m_pOutput { nullptr };
};

template <typename Input, typename Output>
inline TransformTask<Input, Output>::TransformTask(Kernel&& kernel)
    : m_kernel(std::move(kernel))
{
}

template <typename Input, typename Output>
inline void TransformTask<Input, Output>::connect(Task<Output>* pTask)
{
    m_pOutput = pTask;
}

template <typename Input, typename Output>
inline void TransformTask<Input, Output>::execute()
{
    for (std::span<const Input> inputBlock : m_inputStream.consume()) {
        std::vector<Output> output(static_cast<size_t>(inputBlock.size()));
        m_kernel(inputBlock, output);
        if (m_pOutput)
            m_pOutput->enqueue(std::move(output));
    }
}

}

/*#include "task.h"
#include <boost/callable_traits.hpp>
#include <type_traits>

namespace tasking {

auto defaultDataLocalityEstimate = [](int) { return StaticDataInfo {}; };

template <
    typename Kernel,
    typename DataLocalityEstimator,
    typename InputT = std::remove_const_t<std::tuple_element_t<0, boost::callable_traits::args_t<Kernel>>::element_type>,
    typename OutputT = std::tuple_element_t<1, boost::callable_traits::args_t<std::decay_t<Kernel>>>::element_type,
    typename = std::enable_if_t<
        std::is_same_v<std::result_of_t<Kernel(std::span<const InputT>, std::span<OutputT>)>, void> && std::is_same_v<std::result_of_t<DataLocalityEstimator(int)>, StaticDataInfo>>>
class TransformTask : public Task<InputT> {
public:
    TransformTask(TaskPool& pool, Kernel cpuKernel, DataLocalityEstimator dataLocalityEstimator)
        : Task<InputT>(pool, 1)
        , m_kernel(std::forward<Kernel>(cpuKernel))
        , m_dataLocalityEstimator(std::forward<DataLocalityEstimator>(dataLocalityEstimator))
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

private:
    hpx::future<void> execute(DataStream<InputT>& dataStream) const final
    {
        for (auto dataBlock : dataStream.consume()) {
            std::vector<OutputT> output(dataBlock.size());
            m_kernel(dataBlock, output);

            if (m_outputTask) {
                m_outputTask->push(m_outputStreamID, std::move(output));
            }
        }
        co_return;// Important! Creates a coroutine.
    }

    StaticDataInfo staticDataLocalityEstimate(int streamID) const final
    {
        return m_dataLocalityEstimator(streamID);
    }

private:
    Kernel m_kernel;
    DataLocalityEstimator m_dataLocalityEstimator;

    Task<OutputT>* m_outputTask = nullptr;
    int m_outputStreamID = -1;
};

}*/
