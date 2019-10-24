#pragma once
#include "stream/serialize/serializer.h"

namespace stream {

class DummyDeserializer : public stream::Deserializer {
public:
    const void* map(const stream::Allocation&) final { return nullptr; };
    void unmap(const stream::Allocation&) final {};
};
class DummySerializer : public stream::Serializer {
public:
    std::pair<stream::Allocation, void*> allocateAndMap(size_t) final { return { stream::Allocation {}, nullptr }; };
    void unmapPreviousAllocations() final {};

    std::unique_ptr<stream::Deserializer> createDeserializer() final
    {
        return std::make_unique<DummyDeserializer>();
    }
};

}