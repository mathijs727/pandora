#pragma once
#include <cstdint>
#include <gsl/span>
#include "stream/data_stream.h"

namespace tasking::impl {

class TaskBase {
public:
    virtual size_t approximateInputStreamSize() const = 0;
    virtual void execute() = 0;
};

}

namespace tasking {

class TaskPool;

template <typename T>
class Task : public impl::TaskBase {
public:
    uint64_t approximateInputStreamSize() const final;
    void enqueue(std::vector<T>&&);
    virtual void execute() override = 0;

protected:
    DataStream<T> m_inputStream;
};

template <typename T>
inline size_t Task<T>::approximateInputStreamSize() const
{
    return m_inputStream.sizeUnsafe();
}

template <typename T>
inline void Task<T>::enqueue(std::vector<T>&& data)
{
    m_inputStream.push(std::move(data));
}

}