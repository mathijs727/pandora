#include "pandora/utility/memory_arena.h"
//#include "pandora/utility/memory_arena_ts.h"
#include "gtest/gtest.h"
#include <random>
#include <thread>
#include <vector>

using namespace pandora;

struct SomeStruct24Byte // 24 byte
{
    uint64_t item1;
    uint64_t item2;
    double item3;
};

TEST(MemoryArena, FixedAlignment)
{
    MemoryArena allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        SomeStruct24Byte* ptr = allocator.allocate<SomeStruct24Byte, 32>();
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) % 32, 0);
        *ptr = { 0, 0, 0.0f };
    }

    allocator.reset();
}

TEST(MemoryArena, Constructor)
{
    MemoryArena allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        int* ptr = allocator.allocate<int>(42);
        ASSERT_EQ(*ptr, 42);
    }

    allocator.reset();
}

/*TEST(MemoryArena, RandomAlignment)
{
    MemoryArena allocator;

    std::random_device rd;
    std::uniform_int_distribution<int> alignmentBitDist(1, 6);
    for (int i = 0; i < 5000; i++) {
        // Allocate and test random alignment
        int alignment = 1u << alignmentBitDist(rd);
        auto* ptr = allocator.allocate<unsigned char, alignment>(alignment);
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) % alignment, 0);
    }

    allocator.reset();
}

TEST(MemoryArenaTS, FixedSizeAlignment)
{
    MemoryArenaTS allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        auto* ptr = allocator.allocate<SomeStruct24Byte>(32);
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) % 32, 0);
        *ptr = { 0, 0, 0.0f };
    }

    allocator.reset();
}

TEST(MemoryArenaTS, MultiThreadAllocations)
{
    MemoryArenaTS allocator(128);

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([t, &allocator]() {
            std::vector<uint64_t*> ptrs;
            for (int i = 0; i < 2000; i++) {
                auto* ptr = allocator.allocate<uint64_t>(1);
                *ptr = t;
            }

            for (auto ptr : ptrs) {
                ASSERT_EQ(*ptr, t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();

    allocator.reset();
}*/
