#pragma once
#include "data_stream.h"
#include <gsl/gsl>
#include <optional>
#include <tuple>

namespace tasking {

class TaskPool;

struct StaticDataInfo {
    size_t disk = 0;
    size_t mainMemory = 0;
    //size_t gpuMemory;
};

class TaskBase {
public:
    TaskBase(TaskPool& taskPool);

    virtual int numInputStreams() const = 0;

protected:
    friend class TaskPool;

    // Estimation of where the static data lives such that the scheduler can make educated decisions
    // on which tasks to run next.
    virtual StaticDataInfo staticDataLocalityEstimate() const = 0;

    virtual size_t inputStreamSize(int streamID) const = 0;
    virtual void consumeInputStream(int streamID) = 0;
};

class TaskPool {
public:
    void run();

protected:
    friend class TaskBase;
    void registerTask(gsl::not_null<TaskBase*> task);

private:
    std::optional<std::tuple<gsl::not_null<TaskBase*>, int>> getNextTaskToRun() const;

private:
    std::vector<gsl::not_null<TaskBase*>> m_tasks;
};
}
