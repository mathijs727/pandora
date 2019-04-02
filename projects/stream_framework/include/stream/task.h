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
    virtual void execute(DataStream<T>& data) = 0;

private:

    size_t inputStreamSize(int streamID) const final
    {
        return m_inputStreams[streamID].size();
    }

    void consumeInputStream(int streamID) final
    {
        execute(m_inputStreams[streamID]);
    }

private:
    std::vector<DataStream<T>> m_inputStreams;
};

/*class TaskBase {
protected:
    friend class TaskPool;

    virtual void execute() = 0;
    virtual size_t getStreamSize() = 0;
};

template <typename T>
class Task : public TaskBase {
public:
    void pushInputData(gsl::span<const T> data);
protected:
    virtual void execute(gsl::span<const T> input) = 0;
    size_t getStreamSize() override;

private:
    // Called by TaskPool.
    friend class TaskPool;
    void execute();

private:
    DataStream<T> m_inputStream;
};

template <typename T>
inline size_t Task<T>::getStreamSize()
{
    return size_t();
}

template <typename T>
inline void Task<T>::pushInputData(gsl::span<const T> data)
{
    m_inputStream.push(data);
}

template <typename T>
inline void Task<T>::execute()
{
    for (const auto data : m_inputStream.consume()) {
        this->execute(data);
    }
}*/
}
