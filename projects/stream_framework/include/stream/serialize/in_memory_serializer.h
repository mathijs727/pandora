#pragma once
#include "stream/serialize/serializer.h"
#include <cstddef>
#include <vector>

namespace stream {

class InMemoryDeserializer : public stream::Deserializer {
public:
    const void* map(const stream::Allocation&) final;
    void unmap(const stream::Allocation&) final;

private:
    friend class InMemorySerializer;
    InMemoryDeserializer(std::vector<std::byte>&& memory);
    std::vector<std::byte> m_memory;
};

class InMemorySerializer : public stream::Serializer {
public:
    std::pair<stream::Allocation, void*> allocateAndMap(size_t size) final;
    void unmapPreviousAllocations() final;

    std::unique_ptr<stream::Deserializer> createDeserializer() final;

private:
    friend class InMemoryDeserializer;
    struct InMemoryAllocation {
        size_t offset;
	};
    static_assert(sizeof(InMemoryAllocation) <= sizeof(Allocation));
    std::vector<std::byte> m_memory;
};

}