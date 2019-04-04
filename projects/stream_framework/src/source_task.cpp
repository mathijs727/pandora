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

StaticDataInfo SourceTask::staticDataLocalityEstimate(int) const
{
    return StaticDataInfo {};
}

size_t SourceTask::inputStreamSize(int) const
{
    return itemsToProduceUnsafe();
}

hpx::future<void> SourceTask::executeStream(int)
{
    return produce();
}

}
