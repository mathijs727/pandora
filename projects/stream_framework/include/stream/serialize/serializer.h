#pragma once
#include <memory>
#include <tuple>

namespace tasking {

struct Allocation {
    char __memory[24];
};

class Deserializer {
public:
    virtual ~Deserializer() = default;

    virtual const void* map(const Allocation& allocation) = 0;
    virtual void unmap(const void*) = 0;
};

class Serializer {
public:
    virtual ~Serializer() = default;

    virtual std::pair<Allocation, void*> allocateAndMap(size_t numBytes) = 0;
    virtual void unmapPreviousAllocations() = 0;

    virtual std::unique_ptr<Deserializer> createDeserializer() = 0;
};

}