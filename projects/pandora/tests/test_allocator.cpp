#include "pandora/utility/memory_arena.h"
#include "pandora/utility/memory_arena_ts.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
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

TEST(MemoryArenaTS, SingleThreadAllocation)
{
    MemoryArenaTS allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        auto handle = allocator.allocate<uint64_t>(32);
        ASSERT_EQ(handle.get(allocator), 32);
    }

    allocator.reset();
}

TEST(MemoryArenaTS, SingleThreadAllocationN)
{
    MemoryArenaTS allocator(128);

    for (int i = 0; i < 5000; i += 2) {
        // Allocate and test alignment
        auto handle = allocator.allocateN<uint64_t, 2>(0);
		auto& v1 = handle.get(allocator);
		handle++;
		auto& v2 = handle.get(allocator);
		v1 = i;
		v2 = i + 1;
		ASSERT_EQ(v1, i);
		ASSERT_EQ(v2, i + 1);
    }

    allocator.reset();
}

TEST(MemoryArenaTS, SingleThreadAllocationAlignment)
{
	MemoryArenaTS allocator(256);
	struct alignas(32) MyStruct
	{
		uint64_t a1, a2, a3;// 3 * 8 bytes = 24 bytes
	};

	size_t alignment = std::alignment_of_v<MyStruct>;
	(void)alignment;

	for (int i = 0; i < 500; i++) {
		// Allocate and test alignment
		auto handle = allocator.allocate<MyStruct>();
		MyStruct* ptr = &handle.get(allocator);
		ASSERT_EQ(((uintptr_t)ptr) % 32, 0);
	}

	allocator.reset();
}

TEST(MemoryArenaTS, SingleThreadAllocationAlignmentN)
{
	MemoryArenaTS allocator(256);
	struct alignas(32) MyStruct
	{
		uint64_t a1, a2, a3;// 3 * 8 bytes = 24 bytes
	};

	size_t alignment = std::alignment_of_v<MyStruct>;
	(void)alignment;

	for (int i = 0; i < 10; i++) {
		// Allocate and test alignment
		auto handle = allocator.allocateN<MyStruct, 3>();
		MyStruct& ptr1 = (handle++).get(allocator);
		MyStruct& ptr2 = (handle++).get(allocator);
		MyStruct& ptr3 = handle.get(allocator);

		ASSERT_NE(&ptr1, &ptr2);
		ASSERT_NE(&ptr2, &ptr3);
		ASSERT_EQ(((uintptr_t)&ptr1) % alignment, 0);
		ASSERT_EQ(((uintptr_t)&ptr2) % alignment, 0);
		ASSERT_EQ(((uintptr_t)&ptr3) % alignment, 0);
	}

	allocator.reset();
}


TEST(MemoryArenaTS, MultiThreadAllocationSafe)
{
    MemoryArenaTS allocator(128);

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.push_back(std::thread([t, &allocator]() {
            std::vector<MemoryArenaTS::Handle<uint64_t>> handles;
            for (int i = 0; i < 2000; i++) {
                auto handle = allocator.allocate<uint64_t>();
                handle.get(allocator) = t;
                handles.push_back(handle);
            }

            for (auto handle : handles) {
                ASSERT_EQ(handle.get(allocator), t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();

    allocator.reset();
}

TEST(MemoryArenaTS, MultiThreadAllocationUnsafe)
{
    constexpr int numThreads = 4;
    MemoryArenaTS allocator(128);

    std::atomic_int threadsDoneAllocating = numThreads;
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; t++) {
        threads.push_back(std::thread([t, &allocator, &threadsDoneAllocating]() {
            std::vector<MemoryArenaTS::Handle<uint64_t>> handles;
            for (int i = 0; i < 2000; i++) {
                auto handle = allocator.allocate<uint64_t>();
                handle.get(allocator) = t;
                handles.push_back(handle);
            }

            threadsDoneAllocating.fetch_sub(1);
            while (threadsDoneAllocating.load() > 0)
                std::this_thread::sleep_for(std::chrono::microseconds(100));

            for (auto handle : handles) {
                ASSERT_EQ(handle.getUnsafe(allocator), t);
            }
        }));
    }

    for (auto& thread : threads)
        thread.join();

    allocator.reset();
}
