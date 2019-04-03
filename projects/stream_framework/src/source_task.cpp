#include "stream/source_task.h"

namespace tasking {

SourceTask::SourceTask(TaskPool& taskPool)
    : TaskBase(taskPool)
{
}

int SourceTask::numInputStreams() const
{
    return 1;
}

StaticDataInfo SourceTask::staticDataLocalityEstimate() const
{
    return StaticDataInfo {};
}

size_t SourceTask::inputStreamSize(int) const
{
    return itemsToProduce();
}

void SourceTask::consumeInputStream(int)
{
    std::scoped_lock lock { m_mutex };
    produce();
}

}
