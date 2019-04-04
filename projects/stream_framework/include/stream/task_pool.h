#pragma once
#include "data_stream.h"
#include <gsl/gsl>
#include <optional>
#include <tuple>
#include <hpx/include/async.hpp>

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
    virtual StaticDataInfo staticDataLocalityEstimate(int streamID) const = 0;

    virtual size_t inputStreamSize(int streamID) const = 0;
    virtual hpx::future<void> executeStream(int streamID) = 0;
};

class TaskPool {
public:
    void run(int numDevices = std::thread::hardware_concurrency() - 1);

protected:
    friend class TaskBase;
    void registerTask(gsl::not_null<TaskBase*> task);

private:
    std::optional<std::tuple<gsl::not_null<TaskBase*>, int>> getNextTaskToRun();

private:
    std::pair<TaskBase*, int> m_previouslyRunTask{ nullptr, 0 };
    std::vector<gsl::not_null<TaskBase*>> m_tasks;
};
}
