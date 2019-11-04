#include "stream/serialize/file_serializer.h"
#include <gtest/gtest.h>
#include <vector>
#include <optional>

TEST(SplitFileSerializer, WriteAndUnmap)
{
    std::vector<tasking::Allocation> allocations;

	std::unique_ptr<tasking::Deserializer> pDeserializer;
    {
        tasking::SplitFileSerializer serializer { "TEST_WriteAndUnmap", 8 };
        for (int i = 0; i < 8; i++) {
            auto [allocation, pMemory] = serializer.allocateAndMap(sizeof(int));
            const int* pInt = new (pMemory) int(i);
            serializer.unmapPreviousAllocations();

            allocations.push_back(allocation);
        }

		pDeserializer = serializer.createDeserializer();
    }

    for (int i = 0; i < 4; i++) {
        const int* pInt = reinterpret_cast<const int*>(pDeserializer->map(allocations[i]));
        ASSERT_EQ(*pInt, i);
        pDeserializer->unmap(allocations[i]);
    }
}

TEST(SplitFileSerializer, WriteNoUnmap)
{
    std::vector<tasking::Allocation> allocations;

	std::unique_ptr<tasking::Deserializer> pDeserializer;
    {
        tasking::SplitFileSerializer serializer { "TEST_WriteNoUnmap", 8 };
        for (int i = 0; i < 8; i++) {
            auto [allocation, pMemory] = serializer.allocateAndMap(sizeof(int));
            const int* pInt = new (pMemory) int(i);

            allocations.push_back(allocation);
        }

        pDeserializer = serializer.createDeserializer();
    }

    for (int i = 0; i < 4; i++) {
        const int* pInt = reinterpret_cast<const int*>(pDeserializer->map(allocations[i]));
        ASSERT_EQ(*pInt, i);
    }
}
