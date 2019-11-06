#pragma once
#include "stream/stats.h"
#include <EASTL/fixed_vector.h>
#include <functional>
#include <gsl/span>
#include <memory_resource>
#define __TBB_ALLOW_MUTABLE_FUNCTORS 1
#include <tbb/concurrent_queue.h>
#include <tbb/task_group.h>
#undef __TBB_ALLOW_MUTABLE_FUNCTORS
#include <fmt/format.h>
#include <optick/optick.h>
#include <optick/optick_tbb.h>
#include <string>
#include <string_view>
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
    TaskHandle<T> addTask(std::string_view name, Kernel&& kernel);

    // Kernel signature:           void(gsl::span<const T>, std::pmr::memory_resource*>
    // StaticDataLoader signature: StaticData*();
    template <typename T, typename StaticData, typename StaticDataLoader, typename Kernel>
    TaskHandle<T> addTask(std::string_view name, StaticDataLoader&& staticDataLoader, Kernel&& kernel);

    template <typename T>
    void enqueue(TaskHandle<T> task, const T& item);
    template <typename T>
    void enqueue(TaskHandle<T> task, gsl::span<const T> items);

    size_t approxMemoryUsage() const;
    size_t approxQueuedItems() const;

    void run();

private:
    class TaskBase {
    public:
        virtual ~TaskBase() = default;

        virtual size_t approxQueueSize() const = 0;
        virtual size_t approxQueueSizeBytes() const = 0;
        virtual void execute(TaskGraph* pTaskGraph) = 0;
    };
    template <typename T>
    class alignas(64) Task : public TaskBase {
    public:
        template <typename StaticData, typename F1, typename F2>
        static Task<T> initialize(std::string_view name, F1&&, F2&&);
        template <typename Kernel>
        static Task<T> initialize(std::string_view name, Kernel&&);

        ~Task() override = default;

        void enqueue(const T& item);
        void enqueue(gsl::span<const T> items);

        size_t approxQueueSize() const override;
        size_t approxQueueSizeBytes() const override;
        void execute(TaskGraph* pTaskGraph) override;

    private:
        using TypeErasedKernel = std::function<void(gsl::span<T>, const void*, std::pmr::memory_resource*)>;
        using TypeErasedStaticDataLoader = std::function<void*(std::pmr::memory_resource*)>;
        using TypeErasedStaticDataDestructor = std::function<void(std::pmr::memory_resource*, void*)>;

        Task(std::string_view name, TypeErasedKernel&& kernel, TypeErasedStaticDataLoader&& staticData, TypeErasedStaticDataDestructor&& TypeErasedStaticDataDestructor);

    private:
        const std::string m_name;

        const TypeErasedKernel m_kernel;
        const TypeErasedStaticDataLoader m_staticDataLoader;
        const TypeErasedStaticDataDestructor m_staticDataDestructor;
        tbb::concurrent_queue<T> m_workQueue;
    };

    std::vector<std::unique_ptr<TaskBase>> m_tasks;
};

template <typename T, typename Kernel>
inline TaskHandle<T> TaskGraph::addTask(std::string_view name, Kernel&& kernel)
{
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(Task<T>::initialize(name, std::move(kernel)));
    m_tasks.push_back(std::move(pTask));

    return TaskHandle<T> { taskIdx };
}

template <typename T, typename StaticData, typename StaticDataLoader, typename Kernel>
inline TaskHandle<T> TaskGraph::addTask(std::string_view name, StaticDataLoader&& staticDataLoader, Kernel&& kernel)
{
    static_assert(std::is_move_constructible<StaticData>());
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(
        Task<T>::template initialize<StaticData>(name, std::move(kernel), std::move(staticDataLoader)));
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
inline TaskGraph::Task<T> TaskGraph::Task<T>::initialize(std::string_view name, F&& kernel)
{
    return TaskGraph::Task<T>(
        name,
        [=](gsl::span<T> dynamicData, const void*, std::pmr::memory_resource* pMemory) {
            kernel(dynamicData, pMemory);
        },
        [](std::pmr::memory_resource*) { return nullptr; },
        [](std::pmr::memory_resource* pMemoryResource, void*) {});
}

template <typename T>
template <typename StaticData, typename F1, typename F2>
inline TaskGraph::Task<T> TaskGraph::Task<T>::initialize(std::string_view name, F1&& kernel, F2&& staticDataLoader)
{
    return TaskGraph::Task<T>(
        name,
        [=](gsl::span<T> dynamicData, const void* pStaticData, std::pmr::memory_resource* pMemory) {
            kernel(dynamicData, reinterpret_cast<const StaticData*>(pStaticData), pMemory);
        },
        [=](std::pmr::memory_resource* pMemoryResource) -> void* {
            void* pMem = pMemoryResource->allocate(sizeof(StaticData), std::alignment_of_v<StaticData>);
            StaticData copy = staticDataLoader();
            StaticData* pStaticData = new (pMem) StaticData(std::move(copy));
            return reinterpret_cast<void*>(pStaticData);
        },
        [](std::pmr::memory_resource* pMemoryResource, void* pMem) {
            StaticData* pStaticData = reinterpret_cast<StaticData*>(pMem);
            pStaticData->~StaticData();
            pMemoryResource->deallocate(pStaticData, sizeof(StaticData));
        });
}

template <typename T>
inline TaskGraph::Task<T>::Task(
    std::string_view name,
    TaskGraph::Task<T>::TypeErasedKernel&& kernel,
    TaskGraph::Task<T>::TypeErasedStaticDataLoader&& staticDataLoader,
    TaskGraph::Task<T>::TypeErasedStaticDataDestructor&& staticDataDestructor)
    : m_name(name)
    , m_kernel(std::move(kernel))
    , m_staticDataLoader(std::move(staticDataLoader))
    , m_staticDataDestructor(std::move(staticDataDestructor))
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
inline size_t TaskGraph::Task<T>::approxQueueSizeBytes() const
{
    return m_workQueue.unsafe_size() * sizeof(T);
}

template <typename T>
inline void TaskGraph::Task<T>::execute(TaskGraph* pTaskGraph)
{
    StreamStats::FlushInfo flushStats;
    flushStats.taskName = m_name;
    flushStats.genStats = StreamStats::GeneralStats { pTaskGraph->approxQueuedItems(), pTaskGraph->approxMemoryUsage() };
    flushStats.startTime = std::chrono::high_resolution_clock::now();

    std::pmr::memory_resource* pMemory = std::pmr::new_delete_resource();

    void* pStaticData;
    {
        const std::string taskName = fmt::format("{}::staticDataLoad", m_name);
        OPTICK_EVENT_DYNAMIC(taskName.c_str());
        auto stopWatch = flushStats.staticDataLoadTime.getScopedStopwatch();

        // Allocate and construct static data
        pStaticData = m_staticDataLoader(pMemory);
    }

    {
        const std::string taskName = fmt::format("{}::execute", m_name);
        OPTICK_EVENT_DYNAMIC(taskName.c_str());
        auto stopWatch = flushStats.processingTime.getScopedStopwatch();

        tbb::task_group tg;
        eastl::fixed_vector<T, 1024, false> workBatch;
        auto executeKernel = [&]() {
            // Run sequentially in debug mode
            tg.run([this, pStaticData,workBatch = std::move(workBatch),&taskName]() mutable {
                tryRegisterThreadWithOptick();
                OPTICK_EVENT_DYNAMIC(taskName.c_str());

                m_kernel(gsl::make_span(workBatch.data(), workBatch.data() + workBatch.size()), pStaticData, std::pmr::new_delete_resource());
            });
        };

        T workItem;
        while (m_workQueue.try_pop(workItem)) {
            workBatch.push_back(workItem);

            if (workBatch.full()) {
                executeKernel();
                flushStats.itemsFlushed += workBatch.kMaxSize;
                workBatch.clear();
            }
        }
        if (!workBatch.empty()) {
            executeKernel();
            flushStats.itemsFlushed += workBatch.size();
        }

        tg.wait();
    }

    {
        const std::string taskName = fmt::format("{}::staticDataDestruct", m_name);
        OPTICK_EVENT_DYNAMIC(taskName.c_str());
        // Call destructor on static data and free memory
        m_staticDataDestructor(pMemory, pStaticData);
    }

    StreamStats::getSingleton().infoAtFlushes.emplace_back(std::move(flushStats));
}
}