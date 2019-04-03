#pragma once
#include "task.h"
#include <boost/callable_traits.hpp>
#include <type_traits>

namespace tasking {

auto defaultDataLocalityEstimate = []() { return StaticDataInfo {}; };

template <
    typename Kernel,
    typename DataLocalityEstimator,
    typename InputT = std::remove_const_t<std::tuple_element_t<0, boost::callable_traits::args_t<Kernel>>::element_type>,
    typename OutputT = std::tuple_element_t<1, boost::callable_traits::args_t<std::decay_t<Kernel>>>::element_type,
    typename = std::enable_if_t<
        std::is_same_v<std::result_of_t<Kernel(gsl::span<const InputT>, gsl::span<OutputT>)>, void>>>
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

protected:
    void execute(DataStream<InputT>& dataStream) final
    {
        for (auto dataBlock : dataStream.consume()) {
            std::vector<OutputT> output(dataBlock.size());
            m_kernel(dataBlock, output);

            if (m_outputTask) {
                m_outputTask->push(m_outputStreamID, output);
            }
        }
    }

private:
    StaticDataInfo staticDataLocalityEstimate() const override
    {
        return m_dataLocalityEstimator();
    }

private:
    Kernel m_kernel;
    DataLocalityEstimator m_dataLocalityEstimator;

    Task<OutputT>* m_outputTask = nullptr;
    int m_outputStreamID = -1;
};

}
