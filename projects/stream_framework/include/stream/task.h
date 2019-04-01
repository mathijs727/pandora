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
}

}
