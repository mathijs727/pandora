#include "stream/task_pool.h"
#include "spdlog/spdlog.h"
#include <hpx/lcos/async.hpp>
#include <hpx/lcos/channel.hpp>
#include <unordered_map>
#include <tuple>
#include <boost/functional/hash/hash.hpp>

namespace tasking {

void TaskPool::run()
{
    constexpr int numDevices = 4;

    hpx::lcos::local::channel<int> taskRequestChannel;
    std::vector<hpx::lcos::local::channel<int>> taskReceiveChannels(numDevices);

    using ExecutionID = std::pair<int, int>;
    std::unordered_map<ExecutionID, int, boost::hash<ExecutionID>> currentlyExecutingTasks;

    // Spawn a single scheduler task
    hpx::async([&]() -> hpx::future<void> {
        while (true) {
            int deviceID = co_await taskRequestChannel.get();
        }
    });

    /*tbb::flow::graph g;
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
                    else {
                        spdlog::info("DONE");
                        return false; // Program has finished
                    }
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
            task->executeStream(streamID);
            tasksInFlight--;
            return {};
        });

    tbb::flow::make_edge(sourceNode, sinkNode);
    g.wait_for_all();*/
}

void TaskPool::registerTask(gsl::not_null<TaskBase*> task)
{
    m_tasks.push_back(task);
}

std::optional<std::tuple<gsl::not_null<TaskBase*>, int>> TaskPool::getNextTaskToRun()
{
    size_t mostQueuedWork = 0;
    auto [prevRunTask, prevRunStreamID] = m_previouslyRunTask;

    TaskBase* bestTask = nullptr;
    int bestStreamID = 0;
    for (auto task : m_tasks) {
        for (int streamID = 0; streamID < task->numInputStreams(); streamID++) {
            if (task.get() == prevRunTask && streamID == prevRunStreamID)
                continue;

            // TODO(Mathijs): select based on more complex criteria
            if (task->inputStreamSize(streamID) > mostQueuedWork) {
                mostQueuedWork = task->inputStreamSize(streamID);

                bestTask = task;
                bestStreamID = streamID;
            }
        }
    }

    if (mostQueuedWork == 0) {
        if (prevRunTask->inputStreamSize(prevRunStreamID))
            return std::tuple { prevRunTask, prevRunStreamID };
        else
            return {};
    }

    m_previouslyRunTask = { bestTask, bestStreamID };
    return { { bestTask, bestStreamID } };
}

TaskBase::TaskBase(TaskPool& taskPool)
{
    taskPool.registerTask(gsl::make_not_null(this));
}
}
