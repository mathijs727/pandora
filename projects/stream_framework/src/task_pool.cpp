#include "stream/task_pool.h"
#include "spdlog/spdlog.h"
#include <tbb/flow_graph.h>

namespace tasking {

void TaskPool::run()
{
    tbb::flow::graph g;
    std::atomic_int tasksInFlight { 0 };

    using TaskWithStream = std::tuple<TaskBase*, int>;
    using SourceNode = tbb::flow::source_node<TaskWithStream>;
    SourceNode sourceNode(
        g,
        [&](TaskWithStream& out) {
            // NOTE: source node will only run on a single thread at a time.
            while (true) {
                int runningTasks = tasksInFlight;

                auto taskOpt = getNextTaskToRun();
                if (!taskOpt) {
                    if (runningTasks)
                        continue; // Still have running tasks, try again
                    else
                        return false; // Program has finished
                }

                tasksInFlight++;
                auto [taskNotNull, streamID] = *taskOpt;
                out = { taskNotNull.get(), streamID };
                return true;
            }
        });

    using SinkNode = tbb::flow::function_node<TaskWithStream, tbb::flow::continue_msg>;
    SinkNode sinkNode(
        g,
        tbb::flow::concurrency::unlimited,
        [&](TaskWithStream in) -> tbb::flow::continue_msg {
            auto [task, streamID] = in;
            task->consumeInputStream(streamID);
            tasksInFlight--;
            return {};
        });

    tbb::flow::make_edge(sourceNode, sinkNode);
    g.wait_for_all();
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
