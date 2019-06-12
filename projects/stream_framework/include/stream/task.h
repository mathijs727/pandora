#pragma once
#include "task_pool.h"

namespace tasking {

template <typename T>
class Task : public TaskBase {
public:
    Task(TaskPool& taskPool, size_t numInputStreams)
        : TaskBase(taskPool)
        , m_inputStreams(numInputStreams)
    {
    }

    void push(int streamID, gsl::span<const T> data)
    {
        m_inputStreams[streamID].push(data);
    }

    void push(int streamID, std::vector<T>&& data)
    {
        m_inputStreams[streamID].push(data);
    }

    int numInputStreams() const final
    {
        return static_cast<int>(m_inputStreams.size());
    }

protected:
    virtual StaticDataInfo staticDataLocalityEstimate(int streamID) const = 0;
    virtual hpx::future<void> execute(DataStream<T>& dataStream) const = 0;

private:
    size_t inputStreamSize(int streamID) const final
    {
        return m_inputStreams[streamID].sizeUnsafe();
    }

    hpx::future<void> executeStream(int streamID) final
    {
        return execute(m_inputStreams[streamID]);
    }

private:
    std::vector<DataStream<T>> m_inputStreams;
};

}