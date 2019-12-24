#pragma once
#include <thread>
#include <unordered_set>
#include <hpx/lcos/local/mutex.hpp>

namespace tasking {

namespace impl {
    class TaskBase;
}

class TaskPool;
class TaskExecutor {
public:
    TaskExecutor(const TaskPool& taskPool);

    void run(int numDevices = std::thread::hardware_concurrency() - 1);

private:
    impl::TaskBase* getNextTaskToExecute();

private:
    const TaskPool* m_pTaskPool;

    hpx::lcos::local::mutex m_mutex;
    std::unordered_set<impl::TaskBase*> m_currentlyExecutingTasks;
};

}