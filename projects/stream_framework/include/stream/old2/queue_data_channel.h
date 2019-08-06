#pragma once
#include "data_channel.h"
#include <tbb/concurrent_queue.h>

namespace tasking {
template <typename T>
class QueueDataChannel : public DataChannel<T> {
public:
    void pushBlocking(DataBlock<T>&& dataBlock) final;
    std::optional<DataBlock<T>> tryPop() final;

private:
    tbb::concurrent_queue<DataBlock<T>> m_queue;
};

template <typename T>
inline void QueueDataChannel<T>::pushBlocking(DataBlock<T>&& dataBlock)
{
    m_queue.push(std::move(dataBlock));
}

template <typename T>
inline std::optional<DataBlock<T>> QueueDataChannel<T>::tryPop()
{
    DataBlock<T> dataBlock { nullptr };
    bool success = m_queue.try_pop(dataBlock);
    if (success)
        return dataBlock;
    else
        return {};
}

}