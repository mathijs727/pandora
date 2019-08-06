#pragma once
#include <EASTL/fixed_vector.h>
#include <atomic>
#include <functional>
#include <gsl/span>
#include <memory_resource>
#include <tbb/concurrent_queue.h>
#include <tbb/scalable_allocator.h>
#include <tbb/task_arena.h>
#include <tbb/task_group.h>
#include <type_traits>

namespace tasking {

constexpr unsigned max_thread_count = 32;

template <typename T>
struct TaskHandle {
    uint32_t index;
};

class TaskGraph {
public:
    TaskGraph();

    template <typename T>
    using Kernel = std::function<void(gsl::span<const T>, std::pmr::memory_resource* pAllocator)>;
    template <typename T>
    TaskHandle<T> addTask(Kernel<T> kernel);

    template <typename T>
    void enqueueForStart(TaskHandle<T> task, gsl::span<const T>);
    template <typename T>
    void enqueue(TaskHandle<T> task, gsl::span<const T>);

    void run();

private:
    template <typename T>
    struct Task;
    template <typename T>
    void forwardThreadLocalQueue(Task<T>* pTask, int threadIndex);

    template <typename T>
    struct Batch;
    template <typename T>
    Batch<T>* allocateBatch();
    template <typename T>
    void deallocateBatch(Batch<T>* pBatch);

private:
    constexpr static unsigned batch_size_bytes = 128;

    struct alignas(32) BatchMemory {
        unsigned char padding[batch_size_bytes];
    };

    template <typename T>
    struct alignas(32) Batch {
        static_assert(std::alignment_of_v<T> <= 64 && std::alignment_of_v<T> % 2 == 0); // Memory blocks are 64 byte aligned
        static_assert(std::is_trivially_constructible_v<T>);

        static constexpr size_t max_size = (batch_size_bytes - sizeof(size_t)) / sizeof(T);
        static_assert(max_size > 0);

        std::array<T, max_size> data;
        size_t currentSize { 0 };
    };

    struct TaskBase {
        virtual ~TaskBase() = default;

        virtual size_t approxQueueSize() = 0;
        virtual void forwardAllThreadLocalBatchesUnsafe(TaskGraph* pTaskGraph) = 0;
        virtual void flush(TaskGraph* pTaskGraph) = 0;
    };
    template <typename T>
    struct alignas(64) Task : public TaskBase {
        Task(Kernel<T>&& kernel);
        ~Task() override = default;

        size_t approxQueueSize() override;
        void forwardAllThreadLocalBatchesUnsafe(TaskGraph* pTaskGraph) override;
        void flush(TaskGraph* pTaskGraph) override;

        std::array<Batch<T>*, max_thread_count> threadLocalBatches;
        tbb::concurrent_queue<Batch<T>*> fullBatches;

        Kernel<T> kernel;
    };

    tbb::task_arena m_taskArena;
    tbb::scalable_allocator<BatchMemory> m_batchAllocator;
    std::vector<std::unique_ptr<TaskBase>> m_tasks;
};

template <typename T>
inline TaskGraph::Batch<T>* TaskGraph::allocateBatch()
{
    BatchMemory* pMemory = m_batchAllocator.allocate(1);
    return new (pMemory) Batch<T>();
}

template <typename T>
inline void TaskGraph::deallocateBatch(Batch<T>* pBatch)
{
    pBatch->~Batch<T>();
    BatchMemory* pMemory = reinterpret_cast<BatchMemory*>(pBatch);
    m_batchAllocator.deallocate(pMemory, 1);
}

template <typename T>
inline void TaskGraph::forwardThreadLocalQueue(Task<T>* pTask, int threadIndex)
{
    Batch<T>*& pThreadLocalBatch = pTask->threadLocalBatches[threadIndex];
    pTask->fullBatches.push(pThreadLocalBatch);
    pThreadLocalBatch = nullptr;
}

template <typename T>
inline void TaskGraph::enqueueForStart(TaskHandle<T> taskHandle, gsl::span<const T> data)
{
    m_taskArena.execute([&]() {
        enqueue(taskHandle, data);
    });
}

template <typename T>
inline void TaskGraph::enqueue(TaskHandle<T> taskHandle, gsl::span<const T> data)
{
    const int currentThreadIndex = tbb::this_task_arena::current_thread_index();
    Task<T>* pTask = reinterpret_cast<Task<T>*>(m_tasks[taskHandle.index].get());
    Batch<T>*& pThreadLocalBatch = pTask->threadLocalBatches[currentThreadIndex];

    size_t currentIndex = 0;
    while (currentIndex < static_cast<size_t>(data.size())) {
        if (pThreadLocalBatch == nullptr)
            pThreadLocalBatch = allocateBatch<T>();

        const size_t currentBatchSize = pThreadLocalBatch->currentSize;
        const size_t itemsLeftToCopy = data.size() - currentIndex;
        const size_t itemsToCopy = std::min(itemsLeftToCopy, Batch<T>::max_size - currentBatchSize);
        std::copy(
            std::begin(data) + currentIndex,
            std::begin(data) + currentIndex + itemsToCopy,
            std::begin(pThreadLocalBatch->data) + currentBatchSize);
        currentIndex += itemsToCopy;

        pThreadLocalBatch->currentSize += itemsToCopy;
        if (pThreadLocalBatch->currentSize == Batch<T>::max_size)
            forwardThreadLocalQueue<T>(pTask, currentThreadIndex);
    }
}

template <typename T>
inline TaskHandle<T> TaskGraph::addTask(Kernel<T> kernel)
{
    uint32_t taskIdx = static_cast<uint32_t>(m_tasks.size());

    std::unique_ptr<TaskBase> pTask = std::make_unique<Task<T>>(std::move(kernel));
    m_tasks.push_back(std::move(pTask));

    return TaskHandle<T> { taskIdx };
}

template <typename T>
inline TaskGraph::Task<T>::Task(Kernel<T>&& kernel)
    : kernel(std::move(kernel))
{
    std::fill(std::begin(threadLocalBatches), std::end(threadLocalBatches), nullptr);
}

template <typename T>
inline size_t TaskGraph::Task<T>::approxQueueSize()
{
    return fullBatches.unsafe_size();
}

template <typename T>
inline void TaskGraph::Task<T>::forwardAllThreadLocalBatchesUnsafe(TaskGraph* pTaskGraph)
{
    Batch<T>* pSharedBatch = nullptr;

    for (int i = 0; i < tbb::this_task_arena::max_concurrency(); i++) {
        Batch<T>* pThreadLocalBatch = threadLocalBatches[i];
        if (pThreadLocalBatch) {
            const size_t itemsToCopy = pThreadLocalBatch->data.size();

            size_t currentIndex = 0;
            while (currentIndex < pThreadLocalBatch->currentSize) {
                if (pSharedBatch == nullptr)
                    pSharedBatch = pTaskGraph->allocateBatch<T>();

                const size_t currentBatchSize = pSharedBatch->currentSize;
                const size_t itemsLeftToCopy = pThreadLocalBatch->currentSize - currentIndex;
                const size_t itemsToCopy = std::min(itemsLeftToCopy, Batch<T>::max_size - currentBatchSize);
                std::copy(
                    std::begin(pThreadLocalBatch->data) + currentIndex,
                    std::begin(pThreadLocalBatch->data) + currentIndex + itemsToCopy,
                    std::begin(pSharedBatch->data) + currentBatchSize);
                currentIndex += itemsToCopy;

                pSharedBatch->currentSize += itemsToCopy;
                if (pSharedBatch->currentSize == Batch<T>::max_size) {
                    fullBatches.push(pSharedBatch);
                    pSharedBatch = nullptr;
                }
            }

            pTaskGraph->deallocateBatch(pThreadLocalBatch);
        }
        threadLocalBatches[i] = nullptr;
    }

    if (pSharedBatch)
        fullBatches.push(pSharedBatch);
}

template <typename T>
inline void TaskGraph::Task<T>::flush(TaskGraph* pTaskGraph)
{
    tbb::task_group tg;

    Batch<T>* pBatch { nullptr };
    while (fullBatches.try_pop(pBatch)) {
        tg.run([=]() {
            auto pData = pBatch->data.data();
            kernel(gsl::make_span(pData, pData + pBatch->currentSize), std::pmr::new_delete_resource());
            pTaskGraph->deallocateBatch(pBatch);
        });
    }

    tg.wait();
}
}