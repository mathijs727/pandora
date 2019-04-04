#include "stream/task_pool.h"
#include <atomic>
#include <boost/functional/hash/hash.hpp>
#include <hpx/lcos/async.hpp>
#include <hpx/lcos/channel.hpp>
#include <spdlog/spdlog.h>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace tasking {

void TaskPool::run(int numDevices)
{
    using TaskStreamID = std::pair<TaskBase*, int>;
    hpx::lcos::local::channel<int> taskRequestChannel;
    std::vector<hpx::lcos::local::channel<TaskStreamID>> taskReceiveChannels(numDevices);

    // Spawn a single scheduler task
    auto schedulerFuture = hpx::async([&]() {
        std::unordered_set<TaskStreamID, boost::hash<TaskStreamID>> currentlyExecutingTasks;
        std::unordered_map<int, TaskStreamID> currentlyExecutingTaskPerDevice;

        while (true) { // Keep scheduler running
            int deviceID = taskRequestChannel.get().get();

            // The previous task has finished executing.
            if (currentlyExecutingTaskPerDevice.find(deviceID) != std::end(currentlyExecutingTaskPerDevice)) {
                currentlyExecutingTasks.erase(currentlyExecutingTaskPerDevice[deviceID]);
                currentlyExecutingTaskPerDevice.erase(deviceID);
            }

            size_t bestHeuristic = 0;
            TaskBase* bestTask = nullptr;
            int bestStreamID = -1;

            for (const auto task : this->m_tasks) {
                for (int streamID = 0; streamID < task->numInputStreams(); streamID++) {
                    if (currentlyExecutingTasks.find(TaskStreamID { task.get(), streamID }) != std::end(currentlyExecutingTasks))
                        continue;

                    auto heuristic = task->inputStreamSize(streamID);
                    if (heuristic > bestHeuristic) {
                        bestTask = task.get();
                        bestStreamID = streamID;
                    }
                }
            }

            if (bestTask) {
                TaskStreamID executionID { bestTask, bestStreamID };
                currentlyExecutingTasks.insert(executionID);
                currentlyExecutingTaskPerDevice[deviceID] = executionID;
                taskReceiveChannels[deviceID].set(executionID);
            } else {
                if (currentlyExecutingTasks.size()) {
                    // Add the device back to the request channel in the hope that there will be work when we get back to it.
                    // A while loop (waiting for work) would not work because it hang the scheduler, not allowing it to process
                    // notifications from other tasks that they are done processing.
                    taskRequestChannel.set(deviceID);
                } else {
                    // No tasks in flight and all queues are empty => notify worker threads to shut down.
                    for (auto& channel : taskReceiveChannels)
                        channel.set({ nullptr, -1 });

                    return;
                }
            }
        }
    });

    // Spawn a worker task for each device
    std::vector<hpx::future<void>> deviceControlFutures(numDevices);
    for (int deviceID = 0; deviceID < numDevices; deviceID++) {
        deviceControlFutures[deviceID] = hpx::async(
            [&taskRequestChannel, &taskReceiveChannels, deviceID]() {
                while (true) {
                    taskRequestChannel.set(deviceID);
                    auto [task, streamID] = taskReceiveChannels[deviceID].get().get();

                    if (!task)
                        return;

                    task->executeStream(streamID).wait();
                }
            });
    }

    schedulerFuture.wait();
    hpx::wait_all(std::begin(deviceControlFutures), std::end(deviceControlFutures));
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
