#include "stream/task_graph.h"
#include <algorithm>
#include <cassert>
#include <mutex>
#include <optick/optick.h>
#include <optick/optick_tbb.h>
#include <tbb/task_arena.h>

namespace tasking {

TaskGraph::TaskGraph(unsigned numSchedulers)
    : m_numSchedulers(numSchedulers)
{
}

void TaskGraph::run()
{
    tbb::task_group tg;
    std::function<void()> schedule = [&]() {
        TaskBase* pTask { nullptr };
        {
            OPTICK_EVENT("Task Selection");
            auto bestTaskIter = std::max_element(
                std::begin(m_tasks),
                std::end(m_tasks),
                [](const auto& lhs, const auto& rhs) {
                    return lhs->approxQueueSize() < rhs->approxQueueSize();
                });
            assert(bestTaskIter != std::end(m_tasks));

            // TODO: cannot assume this when we allow multiple tasks to execute in parallel (need better stop condition)!
            pTask = bestTaskIter->get();
        }
        if (pTask->approxQueueSize() == 0)
            return;

        pTask->execute(this);
        tg.run(schedule);
    };

    while (!allQueuesEmpty()) {
        for (unsigned i = 0; i < m_numSchedulers; i++) {
            tg.run(schedule);
        }
        tg.wait();
    }
}

size_t TaskGraph::approxMemoryUsage() const
{
    size_t memUsage = 0;
    for (const auto& pTask : m_tasks) {
        memUsage += pTask->approxQueueSizeBytes();
    }
    return memUsage;
}

size_t TaskGraph::approxQueuedItems() const
{
    size_t queuedItems = 0;
    for (const auto& pTask : m_tasks) {
        queuedItems += pTask->approxQueueSize();
    }
    return queuedItems;
}

bool TaskGraph::allQueuesEmpty() const
{
    for (const auto& pTask : m_tasks) {
        if (pTask->approxQueueSize() > 0)
            return false;
    }
    return true;
}

}