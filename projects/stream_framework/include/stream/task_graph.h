#pragma once
#include <EASTL/fixed_vector.h>
#include <functional>
#include <gsl/span>
#include <memory_resource>
#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>
#include <type_traits>

namespace tasking {

template <typename T>
struct TaskHandle {
    uint32_t index;
};

class TaskGraph {
public:
    // Kernel signature: void(gsl::span<const T>, std::pmr::memory_resource*>
    template <typename T, typename Kernel>
    TaskHandle<T> addTask(Kernel&& kernel);

    // Kernel signature:           void(gsl::span<const T>, std::pmr::memory_resource*>
    // StaticDataLoader signature: void(StaticData*);
    template <typename T, typename StaticData, typename Kernel, typename StaticDataLoader>
    TaskHandle<T> addTask(Kernel&& kernel, StaticDataLoader&& staticDataLoader);

    template <typename T>
    void enqueue(TaskHandle<T> task, const T& item);
    template <typename T>
    void enqueue(TaskHandle<T> task, gsl::span<const T> items);

    void run();

private:
    class TaskBase {
    public:
        virtual ~TaskBase() = default;

        virtual size_t approxQueueSize() const = 0;
        virtual void execute(TaskGraph* pTaskGraph) = 0;
    };
    template <typename T>
    class alignas(64) Task : public TaskBase {
    public:
        template <typename StaticData, typename F1, typename F2>
        static Task<T> initialize(F1&&, F2&&);
        template <typename Kernel>
        static Task<T> initialize(Kernel&&);

        ~Task() override = default;

        void enqueue(const T& item);
        void enqueue(gsl::span<const T> items);

        size_t approxQueueSize() const override;
        void execute(TaskGraph* pTaskGraph) override;

    private:
        using TypeErasedKernel = std::function<void(gsl::span<const T>, const void*, std::pmr::memory_resource*)>;
        using TypeErasedStaticDataLoader = std::function<void(void*)>;

        Task(TypeErasedKernel&& kernel, TypeErasedStaticDataLoader&& staticData, size_t staticDataSize, size_t staticDataAlignment);

    private:
        const TypeErasedKernel m_kernel;
        const TypeErasedStaticDataLoader m_staticDataLoader;
        const size_t m_staticDataSize;
        const size_t m_staticDataAlignment;
        tbb::concurrent_queue<T> m_workQueue;
    };

    std::vector<std::unique_ptr<TaskBase>> m_tasks;
};

template <typename T, typename Kernel>
inline TaskHandle<T> TaskGraph::addTask(Kernel&& kernel)
{
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(Task<T>::initialize(std::move(kernel)));
    m_tasks.push_back(std::move(pTask));

    return TaskHandle<T> { taskIdx };
}

template <typename T, typename StaticData, typename Kernel, typename StaticDataLoader>
inline TaskHandle<T> TaskGraph::addTask(Kernel&& kernel, StaticDataLoader&& staticDataLoader)
{
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(
		Task<T>::template initialize<StaticData>(std::move(kernel), std::move(staticDataLoader)));
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
template <typename F>
inline TaskGraph::Task<T> TaskGraph::Task<T>::initialize(F&& kernel)
{
    return TaskGraph::Task<T>(
        [=](gsl::span<const T> dynamicData, const void*, std::pmr::memory_resource* pMemory) {
            kernel(dynamicData, pMemory);
        },
        [](void*) {}, 0, 0);
}

template <typename T>
template <typename StaticData, typename F1, typename F2>
inline TaskGraph::Task<T> TaskGraph::Task<T>::initialize(F1&& kernel, F2&& staticDataLoader)
{
    return TaskGraph::Task<T>(
        [=](gsl::span<const T> dynamicData, const void* pStaticData, std::pmr::memory_resource* pMemory) {
            kernel(dynamicData, reinterpret_cast<const StaticData*>(pStaticData), pMemory);
        },
        [=](void* pStaticData) {
            staticDataLoader(reinterpret_cast<StaticData*>(pStaticData));
        },
        sizeof(StaticData),
        std::alignment_of_v<StaticData>);
}

template <typename T>
inline TaskGraph::Task<T>::Task(
    TaskGraph::Task<T>::TypeErasedKernel&& kernel,
    TaskGraph::Task<T>::TypeErasedStaticDataLoader&& staticDataLoader,
    size_t staticDataSize,
    size_t staticDataAlignment)
    : m_kernel(std::move(kernel))
    , m_staticDataLoader(std::move(staticDataLoader))
    , m_staticDataSize(staticDataSize)
    , m_staticDataAlignment(staticDataAlignment)
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
    std::pmr::memory_resource* pMemory = std::pmr::new_delete_resource();
    void* pStaticData = nullptr;
    if (m_staticDataSize > 0) {
        pStaticData = pMemory->allocate(m_staticDataSize, m_staticDataAlignment);
    }
    m_staticDataLoader(pStaticData);

    tbb::task_group tg;
    eastl::fixed_vector<T, 32, false> workBatch;
    const auto executeKernel = [&]() {
        tg.run([=]() {
            m_kernel(gsl::make_span(workBatch.data(), workBatch.data() + workBatch.size()), pStaticData, std::pmr::new_delete_resource());
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