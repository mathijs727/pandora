#include "stream/task_graph.h"
#include <algorithm>
#include <cassert>

namespace tasking {

void TaskGraph::run()
{
    while (true) {
        auto bestTaskIter = std::max_element(
            std::begin(m_tasks),
            std::end(m_tasks),
            [](const auto& lhs, const auto& rhs) {
                return lhs->approxQueueSize() < rhs->approxQueueSize();
            });
        assert(bestTaskIter != std::end(m_tasks));

		// TODO: cannot assume this when we allow multiple tasks to execute in parallel (need better stop condition)!
        auto& pTask = *bestTaskIter;
        if (pTask->approxQueueSize() == 0)
            break;

        pTask->execute(this);
    }
}

}