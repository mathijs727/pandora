#pragma once
#include <chrono>

class Timer {
    using clock = std::chrono::high_resolution_clock;

public:
    inline Timer()
        : m_start(clock::now())
    {
    }

    template <typename T>
    inline T elapsed() const
    {
        auto now = clock::now();
        return std::chrono::duration_cast<T>(now - m_start);
    }

private:
    const clock::time_point m_start;
};