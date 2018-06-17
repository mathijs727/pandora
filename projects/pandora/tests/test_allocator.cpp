#include "pandora/utility/memory_arena_ts.h"
#include "gtest/gtest.h"
#include <thread>
#include <vector>

using namespace pandora;

struct SomeStruct24Byte // 24 byte
{
    uint64_t item1;
    uint64_t item2;
    double item3;
};

TEST(AllocatorTest, MemoryArenaTS1)
{
    MemoryArenaTS allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        auto* ptr = allocator.allocate<SomeStruct24Byte>(3, 32);
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) % 32, 0);
    }

    allocator.reset();
}

TEST(AllocatorTest, MemoryArenaTS2)
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
}
