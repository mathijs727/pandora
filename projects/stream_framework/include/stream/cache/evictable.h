#pragma once
#include <stream/serialize/serializer.h>

namespace stream {

class Evictable {
public:
    Evictable(bool resident);
    virtual ~Evictable() {};

    virtual size_t sizeBytes() const = 0;
    virtual void serialize(Serializer& serializer) = 0;

    bool isResident() const noexcept;

protected:
    virtual void doEvict() = 0;
    virtual void doMakeResident(Deserializer& deserializer) = 0;

private:
    friend class LRUCache;
    void evict();
    void makeResident(Deserializer& deserializer);

private:
    bool m_isResident;
};

}