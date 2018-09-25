#include "pandora/utility/thread_pool.h"

namespace pandora {

ThreadPool::ThreadPool(int threadCount)
    : m_shouldStop(false)
{
    for (int i = 0; i < threadCount; i++) {
        m_threads.emplace_back([this]() {
            while (!m_shouldStop.load()) {
                std::function<void()> f;
                m_workQueue.pop(f);
                f();
            }
        });
    }
}

ThreadPool::~ThreadPool()
{
    m_shouldStop.store(true);

    for (size_t i = 0; i < m_threads.size(); i++) {
        m_workQueue.push([]() {});// Push dummy item to unblock pop operation
    }

    for (auto& thread : m_threads) {
        thread.join();
    }
}

void ThreadPool::emplace(std::function<void()>&& f)
{
    m_workQueue.emplace(std::move(f));
}

}
