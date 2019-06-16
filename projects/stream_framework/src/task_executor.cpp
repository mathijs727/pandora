#include "stream/task_executor.h"
#include "stream/task_pool.h"
#include <array>
#include <atomic>
#include <chrono>
#include <hpx/async.hpp>
#include <mutex>
#include <spdlog/spdlog.h>

namespace tasking {

TaskExecutor::TaskExecutor(const TaskPool& taskPool)
    : m_pTaskPool(&taskPool)
{
}

void TaskExecutor::run(int numDevices)
{
    std::vector<hpx::future<void>> schedulerTasks;
    for (int i = 0; i < numDevices; i++) {
        hpx::future<void> future = hpx::async([&]() {
            while (impl::TaskBase* pTask = getNextTaskToExecute()) {
                {
                    std::scoped_lock l { m_mutex };
                    m_currentlyExecutingTasks.insert(pTask);
                }

                pTask->execute();

                {
                    std::scoped_lock l { m_mutex };
                    m_currentlyExecutingTasks.erase(pTask);
                }
            }
        });
        schedulerTasks.push_back(std::move(future));
    }

    hpx::wait_all(std::begin(schedulerTasks), std::end(schedulerTasks));
}

impl::TaskBase* TaskExecutor::getNextTaskToExecute()
{
    std::unique_lock l { m_mutex };

    while (true) {
        // Only look for tasks that aren't already executing.
        std::vector<impl::TaskBase*> allowedTasks;
        allowedTasks.reserve(m_pTaskPool->m_tasks.size());
        for (const auto& pTaskOwned : m_pTaskPool->m_tasks) {
            impl::TaskBase* pTask = pTaskOwned.get();
            if (m_currentlyExecutingTasks.find(pTask) == std::end(m_currentlyExecutingTasks))
                allowedTasks.push_back(pTask);
        }

        if (allowedTasks.empty()) {
			// unlock + lock = yield
            l.unlock();
            l.lock();
            continue;
        }

        // Find the task with the most batched items (approximate because other tasks might be pushing to it).
        // TODO: use a better scheduling algorithm.
        impl::TaskBase* pBestTask = *std::max_element(
            std::begin(allowedTasks),
            std::end(allowedTasks),
            [](const auto& lhs, const auto& rhs) {
                return lhs->approximateInputStreamSize() < rhs->approximateInputStreamSize();
            });

        // If no task was found then we keep trying unless nothing was executing at the same time in which case we're done.
        if (pBestTask->approximateInputStreamSize() == 0) {
            if (allowedTasks.size() == m_pTaskPool->m_tasks.size())
                return nullptr;
            else {
                // unlock + lock = yield
                l.unlock();
                l.lock();
                continue;
            }
        }

        return pBestTask;
    }
}

/*void TaskPool::run(int numDevices)
{
    using TaskStreamID = std::pair<TaskBase*, int>;
    hpx::lcos::local::channel<int> taskRequestChannel;
    std::vector<hpx::lcos::local::channel<TaskStreamID>> taskReceiveChannels(numDevices);

    // Spawn a single scheduler task.
    auto schedulerFuture = hpx::async([&]() {
        std::unordered_set<TaskStreamID, boost::hash<TaskStreamID>> currentlyExecutingTasks;
        std::unordered_map<int, TaskStreamID> currentlyExecutingTaskPerDevice;

        while (true) { // Keep scheduler running.
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

    // Spawn a worker task for each device.
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

    taskRequestChannel.close(true); // Clear any outstanding requests for more work to prevent assertion failure (channel must be empty when closed).
}

void TaskPool::registerTask(gsl::not_null<TaskBase*> task)
{
    m_tasks.push_back(task);
}

std::optional<std::tuple<WrappedTask*, int>> TaskPool::getNextTaskToRun()
{
    size_t mostQueuedWork = 0;
    auto [pPrevRunTask, prevRunStreamID] = m_previouslyRunTask;

    WrappedTask* pBestTask = nullptr;
    int bestStreamID = 0;
    for (const auto& task : m_tasks) {
        for (int streamID = 0; streamID < task.numInputStreams(); streamID++) {
			// TODO(Mathijs)
			// Prefer to pick any other task as a hacky way to prevent race conditions.
			// (same task getting picked because its input stream hasn't been fully consumed yet).
            if (&task == pPrevRunTask && streamID == prevRunStreamID)
                continue;

            // TODO(Mathijs): select based on more complex criteria
            auto streamSize = task.inputStreamSize(streamID);
            if (streamSize > mostQueuedWork) {
                mostQueuedWork = streamSize;

                pBestTask = &task;
                bestStreamID = streamID;
            }
        }
    }

    if (mostQueuedWork == 0) {
        if (pPrevRunTask->inputStreamSize(prevRunStreamID))
            return std::tuple { pPrevRunTask, prevRunStreamID };
        else
            return {};
    }

    m_previouslyRunTask = { pBestTask, bestStreamID };
    return { { pBestTask, bestStreamID } };
}

TaskBase::TaskBase(TaskPool& taskPool)
{
    taskPool.registerTask(gsl::make_not_null(this));
}*/
}
