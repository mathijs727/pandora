#pragma once
#include <EASTL/fixed_vector.h>
#include <functional>
#include <gsl/span>
#include <memory_resource>
#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>
#include <memory_resource>

namespace tasking {

constexpr unsigned max_thread_count = 32;

template <typename T>
struct TaskHandle {
    uint32_t index;
};

class TaskGraph {
public:
    template <typename T>
    using Kernel = std::function<void(gsl::span<const T>, std::pmr::memory_resource* pAllocator)>;
    template <typename T>
    TaskHandle<T> addTask(Kernel<T> kernel);

    template <typename T>
    void enqueue(TaskHandle<T> task, const T& item);
    template <typename T>
    void enqueue(TaskHandle<T> task, gsl::span<const T> items);

    void run();

private:
    constexpr static unsigned chunk_size_bytes = 128;

    class TaskBase {
    public:
        virtual ~TaskBase() = default;

        virtual size_t approxQueueSize() const = 0;
        virtual void execute(TaskGraph* pTaskGraph) = 0;
    };
    template <typename T>
    class alignas(64) Task : public TaskBase {
    public:
        Task(Kernel<T>&& kernel);
        ~Task() override = default;

        void enqueue(const T& item);
        void enqueue(gsl::span<const T> items);

        size_t approxQueueSize() const override;
        void execute(TaskGraph* pTaskGraph) override;

    private:
        Kernel<T> m_kernel;
        tbb::concurrent_queue<T> m_workQueue;
    };

    std::vector<std::unique_ptr<TaskBase>> m_tasks;
};

template <typename T>
inline TaskHandle<T> TaskGraph::addTask(Kernel<T> kernel)
{
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(std::move(kernel));
    m_tasks.push_back(std::move(pTask));

    return TaskHandle<T> { taskIdx };
}

template <typename T>
inline void TaskGraph::enqueue(TaskHandle<T> taskHandle, const T& item)
{
    Task<T>* pTask = reinterpret_cast<Task<T>*>(m_tasks[taskHandle.index].get());

    pTask->enqueue(item);
}

template <typename T>
inline void TaskGraph::enqueue(TaskHandle<T> taskHandle, gsl::span<const T> items)
{
    Task<T>* pTask = reinterpret_cast<Task<T>*>(m_tasks[taskHandle.index].get());

    pTask->enqueue(items);
}

template <typename T>
inline TaskGraph::Task<T>::Task(Kernel<T>&& kernel)
    : m_kernel(std::move(kernel))
{
}

template <typename T>
inline void TaskGraph::Task<T>::enqueue(const T& item)
{
    m_workQueue.push(item);
}

template <typename T>
inline void TaskGraph::Task<T>::enqueue(gsl::span<const T> items)
{
    for (const auto& item : items)
        m_workQueue.push(item);
}

template <typename T>
inline size_t TaskGraph::Task<T>::approxQueueSize() const
{
    return m_workQueue.unsafe_size();
}

template <typename T>
inline void TaskGraph::Task<T>::execute(TaskGraph* pTaskGraph)
{
    eastl::fixed_vector<T, 32, false> workBatch;

    tbb::task_group tg;
    const auto executeKernel = [&]() {
        tg.run([=]() {
            m_kernel(gsl::make_span(workBatch.data(), workBatch.data() + workBatch.size()), std::pmr::new_delete_resource());
        });
    };

    T workItem;
    while (m_workQueue.try_pop(workItem)) {
        workBatch.push_back(workItem);

        if (workBatch.full()) {
            executeKernel();
            workBatch.clear();
        }
    }
    if (!workBatch.empty()) {
        executeKernel();
    }

    tg.wait();
}
}