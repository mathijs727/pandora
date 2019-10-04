#include "stream/freelist.h"
#include <gtest/gtest.h>
#include <vector>

TEST(FreeList, AllocateBounded)
{
    struct SimpleData {
        int a;
        float b;
        float c;
        float d;
    };
    tasking::FreeListAllocator<SimpleData> allocator(8, false);
    std::vector<SimpleData*> allocations;
    for (int i = 0; i < 8; i++) {
        SimpleData* pData = allocator.allocate();
        pData->a = i;
        pData->b = static_cast<float>(i);
        pData->c = static_cast<float>(i * 2);
        pData->d = static_cast<float>(i * 3);
        allocations.push_back(pData);
    }
    for (int i = 0; i < 8; i++) {
        SimpleData* pData = allocations[i];
        ASSERT_EQ(pData->a, i);
        ASSERT_EQ(pData->b, static_cast<float>(i));
        ASSERT_EQ(pData->c, static_cast<float>(i * 2));
        ASSERT_EQ(pData->d, static_cast<float>(i * 3));
    }

    ASSERT_THROW(allocator.allocate(), std::exception);
}

TEST(FreeList, AllocateUnbounded)
{
    struct SimpleData {
        int a;
        float b;
        float c;
        float d;
    };
    tasking::FreeListAllocator<SimpleData> allocator(8, true);
    std::vector<SimpleData*> allocations;
    for (int i = 0; i < 16; i++) {
        SimpleData* pData = allocator.allocate();
        pData->a = i;
        pData->b = static_cast<float>(i);
        pData->c = static_cast<float>(i * 2);
        pData->d = static_cast<float>(i * 3);
        allocations.push_back(pData);
    }
    for (int i = 0; i < 16; i++) {
        SimpleData* pData = allocations[i];
        ASSERT_EQ(pData->a, i);
        ASSERT_EQ(pData->b, static_cast<float>(i));
        ASSERT_EQ(pData->c, static_cast<float>(i * 2));
        ASSERT_EQ(pData->d, static_cast<float>(i * 3));
    }
}

TEST(FreeList, Deallocate)
{
    struct SimpleData {
        int a;
        float b;
        float c;
        float d;
    };
    tasking::FreeListAllocator<SimpleData> allocator(8, false);
    std::vector<SimpleData*> allocations;
    for (int i = 0; i < 8; i++) { // Allocate till full
        SimpleData* pData = allocator.allocate();
        pData->a = i;
        pData->b = static_cast<float>(i);
        pData->c = static_cast<float>(i * 2);
        pData->d = static_cast<float>(i * 3);
        allocations.push_back(pData);
    }

    for (int i = 0; i < 4; i++) { // Deallocate to make space
        allocator.deallocate(allocations.back());
        allocations.pop_back();
    }

    for (int i = 0, j = 8; i < 4; i++, j++) { // Allocate again
        SimpleData* pData = allocator.allocate();
        pData->a = j;
        pData->b = static_cast<float>(j);
        pData->c = static_cast<float>(j * 2);
        pData->d = static_cast<float>(j * 3);
        allocations.push_back(pData);
    }

	// Check that the data is still there
    for (int i = 0; i < 4; i++) {
        SimpleData* pData = allocations[i];
        ASSERT_EQ(pData->a, i);
        ASSERT_EQ(pData->b, static_cast<float>(i));
        ASSERT_EQ(pData->c, static_cast<float>(i * 2));
        ASSERT_EQ(pData->d, static_cast<float>(i * 3));
    }
    for (int i = 4, j = 8; i < 8; i++, j++) { // Allocate again
        SimpleData* pData = allocations[i];
        ASSERT_EQ(pData->a, j);
        ASSERT_EQ(pData->b, static_cast<float>(j));
        ASSERT_EQ(pData->c, static_cast<float>(j * 2));
        ASSERT_EQ(pData->d, static_cast<float>(j * 3));
    }
}