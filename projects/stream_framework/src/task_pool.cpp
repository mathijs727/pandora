#include "stream/task_pool.h"
#include "spdlog/spdlog.h"

namespace tasking {

void TaskPool::run()
{
    while (true) {
        auto taskOpt = getNextTaskToRun();
        if (!taskOpt)
            break;

        auto [task, streamID] = *taskOpt;
        task->consumeInputStream(streamID);
    }
}

void TaskPool::registerTask(gsl::not_null<TaskBase*> task)
{
    m_tasks.push_back(task);
}

std::optional<std::tuple<gsl::not_null<TaskBase*>, int>> TaskPool::getNextTaskToRun() const
{
    size_t mostQueuedWork = 0;

    std::optional<std::tuple<gsl::not_null<TaskBase*>, int>> ret;
    for (auto task : m_tasks) {
        for (int streamID = 0; streamID < task->numInputStreams(); streamID++) {
            // TODO(Mathijs): select based on more complex criteria
            if (task->inputStreamSize(streamID) > mostQueuedWork) {
                mostQueuedWork = task->inputStreamSize(streamID);

                ret = std::tuple { task, streamID };
            }
        }
    }

    if (mostQueuedWork == 0)
        return {};

    return ret;
}

TaskBase::TaskBase(TaskPool& taskPool)
{
    taskPool.registerTask(gsl::make_not_null(this));
}
}
