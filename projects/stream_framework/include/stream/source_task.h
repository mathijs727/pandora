#pragma once
#include "task_pool.h"

namespace tasking {

class SourceTask : public TaskBase {
public:
    SourceTask(TaskPool& taskPool);

    int numInputStreams() const final;

protected:
    virtual StaticDataInfo staticDataLocalityEstimate(int streamID) const override;

    virtual hpx::future<void> produce() = 0;
    virtual size_t itemsToProduceUnsafe() const = 0;

private:
    size_t inputStreamSize(int streamID) const final;
    hpx::future<void> executeStream(int streamID) final;
};
}
