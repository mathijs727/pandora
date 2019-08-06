#pragma once
#include "stream/scatter_transform_task.h"
#include "stream/source_task.h"
#include "stream/transform_task.h"
#include <memory>
#include <vector>

namespace tasking {

class TaskExecutor;
class TaskPool {
public:
    template <typename Output, typename Kernel, typename ProductionCountFunc>
    SourceTask<Output>* createSourceTask(Kernel&& kernel, ProductionCountFunc&& productionCountFunc);

    template <typename Input, typename Output, typename F>
    TransformTask<Input, Output>* createTransformTask(F&& kernel);

    template <typename Input, typename Output, typename F>
    ScatterTransformTask<Input, Output>* createScatterTransformTask(F&& kernel, unsigned numOutputs);

private:
    friend class TaskExecutor;

    std::vector<std::unique_ptr<impl::TaskBase>> m_tasks;
};

template <typename Output, typename Kernel, typename ProductionCountFunc>
inline SourceTask<Output>* TaskPool::createSourceTask(Kernel&& kernel, ProductionCountFunc&& productionCountFunc)
{
    // Don't use make_unique because it doesn't allow us to access the private (friended) constructor.
    auto pTask = std::unique_ptr<SourceTask<Output>>(new SourceTask<Output>(std::forward<Kernel>(kernel), std::forward<ProductionCountFunc>(productionCountFunc)));
    auto ret = pTask.get();
    m_tasks.push_back(std::move(pTask));
    return ret;
}

template <typename Input, typename Output, typename F>
inline TransformTask<Input, Output>* TaskPool::createTransformTask(F&& kernel)
{
    // Don't use make_unique because it doesn't allow us to access the private (friended) constructor.
    auto pTask = std::unique_ptr<TransformTask<Input, Output>>(new TransformTask<Input, Output>(std::forward<F>(kernel)));
    auto ret = pTask.get();
    m_tasks.push_back(std::move(pTask));
    return ret;
}

template <typename Input, typename Output, typename F>
inline ScatterTransformTask<Input, Output>* TaskPool::createScatterTransformTask(F&& kernel, unsigned numOutputs)
{
    // Don't use make_unique because it doesn't allow us to access the private (friended) constructor.
    auto pTask = std::unique_ptr<ScatterTransformTask<Input, Output>> (new ScatterTransformTask<Input, Output>(std::forward<F>(kernel), numOutputs));
    auto ret = pTask.get();
    m_tasks.push_back(std::move(pTask));
    return ret;
}

}
