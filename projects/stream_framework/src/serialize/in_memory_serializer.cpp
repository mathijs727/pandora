#include "stream/serialize/in_memory_serializer.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace tasking {

const void* InMemoryDeserializer::map(const Allocation& allocation)
{
    InMemorySerializer::InMemoryAllocation inMemoryAllocation;
    std::memcpy(&inMemoryAllocation, &allocation, sizeof(decltype(inMemoryAllocation)));

    return reinterpret_cast<void*>(&m_memory[inMemoryAllocation.offset]);
}

void InMemoryDeserializer::unmap(const Allocation& allocation)
{
}

std::pair<Allocation, void*> InMemorySerializer::allocateAndMap(size_t numBytes)
{
    const size_t offset = m_memory.size();
    m_memory.resize(offset + numBytes);
    void* pMemory = reinterpret_cast<void*>(m_memory.data() + offset);

    InMemoryAllocation inMemoryAllocation { offset };
    Allocation allocation;
    std::memcpy(&allocation, &inMemoryAllocation, sizeof(InMemoryAllocation));

    return { allocation, pMemory };
}

void InMemorySerializer::unmapPreviousAllocations()
{
}

std::unique_ptr<Deserializer> InMemorySerializer::createDeserializer()
{
    // Cannot use make_unique with private constructors
    return std::unique_ptr<InMemoryDeserializer>(new InMemoryDeserializer(std::move(m_memory)));
}

InMemoryDeserializer::InMemoryDeserializer(std::vector<std::byte>&& memory)
    : m_memory(std::move(memory))
{
}

}