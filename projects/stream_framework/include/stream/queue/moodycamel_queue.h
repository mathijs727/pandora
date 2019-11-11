#pragma once
#include "moodycamel/concurrentqueue.h"

namespace tasking {

template <typename T>
class MoodyCamelQueue : public moodycamel::ConcurrentQueue<T> {
public:
    MoodyCamelQueue(MoodyCamelQueue<T>&& other)
        : moodycamel::ConcurrentQueue<T>(std::move(other))
    {
    }

    MoodyCamelQueue()
        : moodycamel::ConcurrentQueue<T>()
    {
    }

    void inline push(T&& item)
    {
        this->enqueue(std::move(item));
    }
    void inline push(const T& item)
    {
        this->enqueue(item);
    }

    bool inline try_pop(T& item)
    {
        return this->try_dequeue(item);
    }

    size_t inline unsafe_size() const
    {
        return this->size_approx();
    }
};

}