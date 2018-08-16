#include "pandora/utility/growing_free_list_ts.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <random>
#include <thread>
#include <vector>

using namespace pandora;

TEST(GrowingFreeListTS, SingleThreadAllocationDeallocation)
{
    GrowingFreeListTS<uint64_t> allocator;

    auto* valPtr = allocator.allocate(420);
    ASSERT_EQ(*valPtr, 420);

    auto* valPtr2 = allocator.allocate(69);
    ASSERT_EQ(*valPtr2, 69);

    allocator.deallocate(valPtr2);

    valPtr2 = allocator.allocate(17);
    ASSERT_EQ(*valPtr2, 17);
}

TEST(GrowingFreeListTS, SingleThreadAllocationAlignment)
{
    struct alignas(32) MyStruct {
        MyStruct(uint64_t a1, uint64_t a2, uint64_t a3)
            : a1(a1)
            , a2(a2)
            , a3(a3)
        {
        }
        uint64_t a1, a2, a3; // 3 * 8 bytes = 24 bytes
    };
    size_t alignment = std::alignment_of_v<MyStruct>;

    GrowingFreeListTS<MyStruct> allocator;
    auto* p = allocator.allocate(2, 3, 4);
    ASSERT_EQ(p->a1, 2);
    ASSERT_EQ(p->a2, 3);
    ASSERT_EQ(p->a3, 4);
    ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0);
}

TEST(GrowingFreeListTS, MultiThreadAllocation)
{
    GrowingFreeListTS<uint64_t> allocator;

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([t, &allocator]() {
            std::vector<uint64_t*> items;
            for (int i = 0; i < 2000; i++) {
                auto* item = allocator.allocate(42);
                ASSERT_EQ(*item, 42);
                *item = t;
                items.push_back(item);
            }

            for (auto item : items) {
                ASSERT_EQ(*item, t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();
}

