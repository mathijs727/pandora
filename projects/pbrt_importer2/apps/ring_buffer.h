#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

// https://github.com/Vitorian/RedditHelp/blob/master/test_spsc_ring.cpp
template <typename T, typename IndexT = uint32_t, typename AllocT = std::allocator<T>>
class VitorianRing {
public:
    static constexpr uint32_t SLOTSIZE = sizeof(T);
    VitorianRing(uint32_t sz)
        : size(sz)
    {
        data = allocator.allocate(sz);
        read_idx = 0;
        write_idx = 2 * size;
    }
    ~VitorianRing()
    {
        allocator.deallocate(data, size);
    }

    bool push(const T& obj)
    {
        uint32_t numel = (write_idx - read_idx) % (2 * size);
        //fprintf( stderr, "Push read:%d write:%d diff:%d\n", read_idx, write_idx, numel );
        if (numel < size) {
            data[write_idx % size] = obj;
            write_idx = (write_idx + 1 < 4 * size) ? write_idx + 1 : 2 * size;
            return true;
        }
        return false;
    }
    bool pop(T& obj)
    {
        uint32_t numel = (write_idx - read_idx) % (2 * size);
        //fprintf( stderr, "Pop  read:%d write:%d diff:%d\n", read_idx, write_idx, numel );
        if (numel > 0) {
            obj = data[read_idx % size];
            read_idx = (read_idx + 1 < 2 * size) ? read_idx + 1 : 0;
            return true;
        }
        return false;
    }

private:
    VitorianRing();
    VitorianRing(const VitorianRing&);
    std::atomic<IndexT> read_idx;
    std::atomic<IndexT> write_idx;
    const uint32_t size;
    T* data;
    AllocT allocator;
};