#include "stream/task_graph.h"
#include <algorithm>
#include <cassert>
#include <thread>

namespace tasking {

TaskGraph::TaskGraph()
    : m_taskArena(static_cast<int>(std::thread::hardware_concurrency()))
{
}

void TaskGraph::run()
{
    m_taskArena.execute([&]() {
        while (true) {
            std::for_each(std::begin(m_tasks), std::end(m_tasks), [&](auto& pTask) {
                pTask->forwardAllThreadLocalBatchesUnsafe(this);
            });

            auto bestTaskIter = std::max_element(
                std::begin(m_tasks),
                std::end(m_tasks),
                [](const auto& lhs, const auto& rhs) {
                    return lhs->approxQueueSize() < rhs->approxQueueSize();
                });
            assert(bestTaskIter != std::end(m_tasks));

            auto& pTask = *bestTaskIter;
            if (pTask->approxQueueSize() == 0)
                break;
            pTask->flush(this);
        }
    });
}

}