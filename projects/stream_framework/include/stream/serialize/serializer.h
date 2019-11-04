#pragma once
#include <tuple>
#include <memory>

namespace tasking {

struct Allocation {
    char __memory[16];
};

class Deserializer {
public:
    virtual const void* map(const Allocation& allocation) = 0;
    virtual void unmap(const Allocation& allocation) = 0;
};

class Serializer {
public:
    virtual std::pair<Allocation, void*> allocateAndMap(size_t numBytes) = 0;
    virtual void unmapPreviousAllocations() = 0;

	virtual std::unique_ptr<Deserializer> createDeserializer() = 0;
};

}