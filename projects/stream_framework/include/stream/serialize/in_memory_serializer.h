#pragma once
#include "stream/serialize/serializer.h"
#include <cstddef>
#include <vector>

namespace tasking {

class InMemoryDeserializer : public tasking::Deserializer {
public:
    const void* map(const tasking::Allocation&) final;
    void unmap(const void*) final;

private:
    friend class InMemorySerializer;
    InMemoryDeserializer(std::vector<std::byte>&& memory);
    std::vector<std::byte> m_memory;
};

class InMemorySerializer : public tasking::Serializer {
public:
    std::pair<tasking::Allocation, void*> allocateAndMap(size_t size) final;
    void unmapPreviousAllocations() final;

    std::unique_ptr<tasking::Deserializer> createDeserializer() final;

private:
    friend class InMemoryDeserializer;
    struct InMemoryAllocation {
        size_t offset;
	};
    static_assert(sizeof(InMemoryAllocation) <= sizeof(Allocation));
    std::vector<std::byte> m_memory;
};

}