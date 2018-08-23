#include "pandora/utility/free_list_backed_memory_arena.h"
#include "pandora/utility/growing_free_list_ts.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

using namespace pandora;

using MyMemoryArena = FreeListBackedMemoryArena<128>;

TEST(FreeListBackedMemoryArena, SingleThreadAllocation)
{
    GrowingFreeListTS<MyMemoryArena::MemoryBlock> backingAllocator;
    MyMemoryArena allocator = { backingAllocator };

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        const auto* ptr = allocator.allocate<uint64_t>(32);
        ASSERT_EQ(*ptr, 32);
    }
}

TEST(FreeListBackedMemoryArena, SingleThreadAllocationAlignment)
{
    GrowingFreeListTS<MyMemoryArena::MemoryBlock> backingAllocator;
    MyMemoryArena allocator = { backingAllocator };

    struct alignas(32) MyStruct {
        uint64_t a1, a2, a3; // 3 * 8 bytes = 24 bytes
    };

    constexpr size_t alignment = std::alignment_of_v<MyStruct>;
    ASSERT_EQ(alignment, 32);

    for (int i = 0; i < 500; i++) {
        // Allocate and test alignment
        const auto* ptr = allocator.allocate<MyStruct>();
        ASSERT_EQ(((uintptr_t)ptr) % alignment, 0);
    }
}

TEST(FreeListBackedMemoryArena, MultiThreadAllocation)
{
    GrowingFreeListTS<MyMemoryArena::MemoryBlock> backingAllocator;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([t, &backingAllocator]() {
            MyMemoryArena allocator = { backingAllocator };

            std::vector<uint64_t*> pointers;
            for (int i = 0; i < 2000; i++) {
                auto* ptr = allocator.allocate<uint64_t>();
                *ptr = t;
                pointers.push_back(ptr);
            }

            for (const auto* ptr : pointers) {
                ASSERT_EQ(*ptr, t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();
}