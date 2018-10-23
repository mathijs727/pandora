#pragma once
#include <atomic>
#include <functional>
#include <tbb/concurrent_queue.h>
#include <thread>
#include <vector>

namespace pandora {

class ThreadPool {
public:
    ThreadPool(int threadCount);
    ~ThreadPool();

    void emplace(std::function<void()>&& f);

private:
    // Unlke concurrent_queue, this does provide a blocking pop. Also helps us control the flow of the program
    tbb::concurrent_bounded_queue<std::function<void()>> m_workQueue;
    std::atomic_bool m_shouldStop;
    std::vector<std::thread> m_threads;
};

}
