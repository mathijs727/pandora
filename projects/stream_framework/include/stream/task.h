#pragma once
#include "data_stream.h"
#include <gsl/gsl>

namespace tasking {

class TaskPool;

class TaskBase {
protected:
    friend class TaskPool;

    virtual void execute() = 0;
    virtual size_t getStreamSize() = 0;
};

template <typename T>
class Task : public TaskBase {
protected:
    virtual void execute() = 0;
    size_t getStreamSize() override;

private:
    // Called by TaskPool.
    friend class TaskPool;
    void pushInputData(gsl::span<const T> data);

    template <typename T1, typename T2>
    friend class TransformTask;
    template <typename T1, typename T2>
    friend class FilteredTransformTask;
    template <typename T1, typename T2>
    friend class MultiTypeScatterTransformTask;
    DataStream<T>& getInputStreamReference();

private:
    DataStream<T> m_inputStream { 32 * 1024 * 1024 };
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
inline DataStream<T>& Task<T>::getInputStreamReference()
{
    return m_inputStream;
}

}