#pragma once
#include "stream/serialize/serializer.h"

namespace tasking {

class DummyDeserializer : public tasking::Deserializer {
public:
    const void* map(const tasking::Allocation&) final { return nullptr; };
    void unmap(const tasking::Allocation&) final {};
};
class DummySerializer : public tasking::Serializer {
public:
    std::pair<tasking::Allocation, void*> allocateAndMap(size_t) final { return { tasking::Allocation {}, nullptr }; };
    void unmapPreviousAllocations() final {};

    std::unique_ptr<tasking::Deserializer> createDeserializer() final
    {
        return std::make_unique<DummyDeserializer>();
    }
};

}