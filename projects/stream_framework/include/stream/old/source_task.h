#pragma once
#include "stream/task.h"
#include <functional>

namespace tasking {

template <typename Output>
class SourceTask : public impl::TaskBase {
public:
    void connect(Task<Output>* pOutput);

private:
    friend class TaskPool;
    using Kernel = std::function<void(std::span<Output>)>;
    using ProductionCountFunc = std::function<size_t()>;
    SourceTask(Kernel&& kernel, ProductionCountFunc&& productionCountFunc);

private:
    Kernel m_kernel;
    ProductionCountFunc m_productionCountFunc;

    Task<Output>* m_pOutput { nullptr };
};

template <typename Output>
inline SourceTask<Output>::SourceTask(Kernel&& kernel, ProductionCountFunc&& productionCountFunc)
    : m_kernel(std::move(kernel))
    , m_productionCountFunc(std::move(productionCountFunc))
{
}

template <typename Output>
inline void SourceTask<Output>::connect(Task<Output>* pOutput)
{
    m_pOutput = pOutput;
}

template <typename Output>
inline size_t SourceTask<Output>::approximateInputStreamSize() const
{
    return m_productionCountFunc();
}

template <typename Output>
inline void SourceTask<Output>::execute()
{
    size_t outputCount = m_productionCountFunc();

    std::vector<Output> output(outputCount);
    m_kernel(output);
    m_pOutput->enqueue(std::move(output));
}

}
