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

size_t SourceTask::inputStreamSize(int) const
{
    return itemsToProduce();
}

void SourceTask::consumeInputStream(int)
{
    produce();
}

}