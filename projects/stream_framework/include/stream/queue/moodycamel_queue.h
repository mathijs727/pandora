#pragma once
#include "moodycamel/concurrentqueue.h"
#include <tbb/task_arena.h>
#include <thread>
#include <vector>

namespace tasking {

template <typename T>
class MoodyCamelQueue : public moodycamel::ConcurrentQueue<T> {
public:
    MoodyCamelQueue(MoodyCamelQueue<T>&& other)
        : moodycamel::ConcurrentQueue<T>(std::move(other))
        , m_producerTokens(std::move(other.m_producerTokens))
        , m_consumerTokens(std::move(other.m_consumerTokens))
    {
    }

    MoodyCamelQueue()
        : moodycamel::ConcurrentQueue<T>()
    {
        std::generate_n(
            std::back_inserter(m_producerTokens),
            std::thread::hardware_concurrency(),
            [&]() {
                return moodycamel::ProducerToken(*this);
            });
        std::generate_n(
            std::back_inserter(m_consumerTokens),
            std::thread::hardware_concurrency(),
            [&]() {
                return moodycamel::ConsumerToken(*this);
            });
    }

    void inline push(T&& item)
    {
        auto threadIdx = tbb::this_task_arena::current_thread_index();
        auto& token = m_producerTokens[threadIdx];
        this->enqueue(token, std::move(item));
        //this->enqueue(std::move(item));
    }
    void inline push(const T& item)
    {
        auto threadIdx = tbb::this_task_arena::current_thread_index();
        auto& token = m_producerTokens[threadIdx];
        this->enqueue(token, item);
        //this->enqueue(item);
    }

    bool inline try_pop(T& item)
    {
        auto threadIdx = tbb::this_task_arena::current_thread_index();
        auto& token = m_consumerTokens[threadIdx];
        return this->try_dequeue(token, item);
        //return this->try_dequeue(item);
    }

    size_t inline unsafe_size() const
    {
        return this->size_approx();
    }

    size_t inline unsafe_size_bytes() const
    {
        return this->m_sizeBytes;
    }

private:
    std::vector<moodycamel::ProducerToken> m_producerTokens;
    std::vector<moodycamel::ConsumerToken> m_consumerTokens;
};

}