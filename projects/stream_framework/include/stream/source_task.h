#pragma once
#include "task_pool.h"

namespace tasking {

class SourceTask : public TaskBase {
public:
    SourceTask(TaskPool& taskPool);

    int numInputStreams() const final;
protected:
    virtual void produce() = 0;
    virtual size_t itemsToProduce() const = 0;

private:
    size_t inputStreamSize(int streamID) const final;
    void consumeInputStream(int streamID) final;
};
}
