#include "stream/task_graph.h"
#include <algorithm>
#include <cassert>
#include <mutex>
#include <optick.h>
#include <optick_tbb.h>
#include <thread>

namespace tasking {

TaskGraph::TaskGraph(unsigned numSchedulers)
    : m_numSchedulers(numSchedulers)
    , m_taskArena(static_cast<int>(std::thread::hardware_concurrency()))
{
    auto& stats = StreamStats::getSingleton();
    (void)stats;
}

void TaskGraph::run()
{
    m_inTaskArena = true;
    m_taskArena.execute([&] {
        tbb::task_group tg;
        std::function<void()> schedule = [&]() {
            Optick::tryRegisterThreadWithOptick();

            TaskBase* pTask { nullptr };
            {
                OPTICK_EVENT("Task Selection");
                auto bestTaskIter = std::max_element(
                    std::begin(m_tasks),
                    std::end(m_tasks),
                    [](const auto& lhs, const auto& rhs) {
                        if (lhs->isExecuting())
                            return true;
                        if (rhs->isExecuting())
                            return false;
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
    });
    m_inTaskArena = false;
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