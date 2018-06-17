// Like std::stack but using memory that is stored in stack memory instead of heap memory
// Inspired by EASTLs fixed_vector:
// https://github.com/electronicarts/EASTL/blob/master/include/EASTL/fixed_vector.h

#pragma once
#include <array>

namespace pandora {
template <typename T, size_t maxSize>
class fixed_stack {
public:
    fixed_stack()
        : m_currentIndex(0)
    {
    }

    void push(const T& item)
    {
        assert(m_currentIndex < maxSize); // stack is full
        m_data[m_currentIndex++] = item;
    }

    T pop()
    {
        assert(m_currentIndex > 0);
        return m_data[--m_currentIndex];
    }

    bool empty() { return m_currentIndex == 0; }

private:
    std::array<T, maxSize> m_data;
    size_t m_currentIndex;
};
}
