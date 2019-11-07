#pragma once
#include <tbb/concurrent_queue.h>

namespace tasking {

template <typename T>
class Queue : public tbb::concurrent_queue<T> {
};

}