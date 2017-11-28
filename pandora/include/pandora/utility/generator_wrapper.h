#pragma once

namespace pandora {

template <class T>
class GeneratorWrapper {
public:
    GeneratorWrapper(T begin, T end)
        : m_begin(begin)
        , m_end(end)
    {
    }
    T begin() { return m_begin; }
    T end() { return m_end; }

private:
    T m_begin;
    T m_end;
};
}