#include "pandora/utility/blocked_stack_allocator.h"
#include "gtest/gtest.h"

using namespace pandora;

struct SomeStruct24Byte // 24 byte
{
    uint64_t item1;
    uint64_t item2;
    double item3;
};

TEST(AllocatorTest, BlockedStackAllocator)
{
    BlockedStackAllocator allocator;

    for (int i = 0; i < 5000; i++) {
        auto* ptr = allocator.allocate<SomeStruct24Byte>(3, 32);
        ASSERT_EQ(reinterpret_cast<size_t>(ptr) % 32, 0);
    }

    allocator.reset();
}