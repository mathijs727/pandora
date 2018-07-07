#include "pandora/utility/memory_arena.h"
#include "gtest/gtest.h"

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

TEST(MemoryArena, Allocate)
{
    MemoryArena allocator;

    for (int i = 0; i < 5000; i++) {
        // Allocate and test alignment
        int* ptr = allocator.allocate<int>(42);
        ASSERT_EQ(*ptr, 42);
    }

    allocator.reset();
}
