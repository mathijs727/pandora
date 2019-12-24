#pragma once
#include <cstddef>
#include <tuple>

// https://stackoverflow.com/questions/11328264/python-like-loop-enumeration-in-c
template <typename Iterable>
class enumerate_object {
private:
    Iterable _iter;
    std::size_t _size;
    decltype(std::begin(_iter)) _begin;
    const decltype(std::end(_iter)) _end;

public:
    enumerate_object(Iterable iter)
        : _iter(iter)
        , _size(0)
        , _begin(std::begin(iter))
        , _end(std::end(iter))
    {
    }

    const enumerate_object& begin() const { return *this; }
    const enumerate_object& end() const { return *this; }

    bool operator!=(const enumerate_object&) const
    {
        return _begin != _end;
    }

    void operator++()
    {
        ++_begin;
        ++_size;
    }

    auto operator*() const -> std::pair<std::size_t, decltype(*_begin)>
    {
        return { _size, *_begin };
    }
};

template <typename Iterable>
auto enumerate(Iterable&& iter) -> enumerate_object<Iterable>
{
    return { std::forward<Iterable>(iter) };
}