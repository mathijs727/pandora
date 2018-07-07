#include "pandora/utility/contiguous_allocator_ts.h"
#include "pandora/utility/memory_arena_ts.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <vector>

using namespace pandora;

TEST(ContiguousAllocatorTS, SingleThreadAllocation)
{
    ContiguousAllocatorTS<uint64_t> allocator(5000, 100);

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        auto [handle, ptr] = allocator.allocate(32);
        ASSERT_EQ(&allocator.get(handle), ptr);
        ASSERT_EQ(*ptr, 32);
    }
}

TEST(ContiguousAllocatorTS, SingleThreadAllocationN)
{
    ContiguousAllocatorTS<uint64_t> allocator(5000, 100);

    for (int i = 0; i < 5000; i += 2) {
        // Allocate and test alignment
        auto [handle, ptr] = allocator.allocateN<2>(3);
        auto& v1 = allocator.get(handle);
        auto& v2 = allocator.get(++handle);

        ASSERT_EQ(v1, 3);
        ASSERT_EQ(v2, 3);

        v1 = i;
        v2 = i + 1;
        ASSERT_EQ(v1, i);
        ASSERT_EQ(v2, i + 1);
    }
}

TEST(ContiguousAllocatorTS, SingleThreadAllocationAlignment)
{
    struct alignas(32) MyStruct {
        uint64_t a1, a2, a3; // 3 * 8 bytes = 24 bytes
    };
    ContiguousAllocatorTS<MyStruct> allocator(500, 100);

    size_t alignment = std::alignment_of_v<MyStruct>;

    for (int i = 0; i < 500; i++) {
        // Allocate and test alignment
        auto [handle, ptr] = allocator.allocate();
        ASSERT_EQ(&allocator.get(handle), ptr);
        ASSERT_EQ(((uintptr_t)ptr) % alignment, 0);
    }
}

TEST(ContiguousAllocatorTS, SingleThreadAllocationAlignmentN)
{
    struct alignas(32) MyStruct {
        uint64_t a1, a2, a3; // 3 * 8 bytes = 24 bytes
    };
    ContiguousAllocatorTS<MyStruct> allocator(40, 10);// We allocate 3 items at a time (9 per block) so need some extra space

    size_t alignment = std::alignment_of_v<MyStruct>;

    for (int i = 0; i < 10; i++) {
        // Allocate and test alignment
        auto [handle, ptr] = allocator.allocateN<3>();
        MyStruct* ptr1 = &allocator.get(handle);
        MyStruct* ptr2 = &allocator.get(++handle);
        MyStruct* ptr3 = &allocator.get(++handle);

        ASSERT_EQ((ptr + 0), ptr1);
        ASSERT_EQ((ptr + 1), ptr2);
        ASSERT_EQ((ptr + 2), ptr3);

        ASSERT_NE(ptr1, ptr2);
        ASSERT_NE(ptr2, ptr3);
        ASSERT_EQ(((uintptr_t)ptr1) % alignment, 0);
        ASSERT_EQ(((uintptr_t)ptr2) % alignment, 0);
        ASSERT_EQ(((uintptr_t)ptr3) % alignment, 0);
    }
}

TEST(ContiguousAllocatorTS, MultiThreadAllocationSafe)
{
    const int numThreads = 4;
    ContiguousAllocatorTS<uint64_t> allocator(numThreads * (2000 + 50), 50);

    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.push_back(std::thread([t, &allocator]() {
            std::vector<ContiguousAllocatorTS<uint64_t>::Handle> handles;
            for (int i = 0; i < 2000; i++) {
                auto [handle, _] = allocator.allocate();
                allocator.get(handle) = t;
                handles.push_back(handle);
            }

            for (auto handle : handles) {
                ASSERT_EQ(allocator.get(handle), t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();
}

TEST(ContiguousAllocatorTS, MultiThreadAllocationUnsafe)
{
    const int numThreads = 4;
	ContiguousAllocatorTS<uint64_t> allocator(numThreads * (2000 + 50), 50);

    std::atomic_int threadsDoneAllocating = numThreads;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.push_back(std::thread([t, &allocator, &threadsDoneAllocating]() {
            std::vector<ContiguousAllocatorTS<uint64_t>::Handle> handles;
            for (int i = 0; i < 2000; i++) {
                auto [handle, _] = allocator.allocate();
                allocator.get(handle) = t;
                handles.push_back(handle);
            }

            threadsDoneAllocating.fetch_sub(1);
            while (threadsDoneAllocating.load() > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));

            for (auto handle : handles) {
                ASSERT_EQ(allocator.get(handle), t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();
}
