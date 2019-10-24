#pragma once

namespace stream {

class Serializer;
class Deserializer;

class Evictable {
public:
    Evictable(bool resident);
    virtual ~Evictable() {};

    virtual size_t sizeBytes() const = 0;
    virtual void serialize(Serializer& serializer) const = 0;

    void evict();
    void makeResident(Deserializer& deserializer);

    bool isResident() const noexcept;

protected:
    virtual void doEvict() = 0;
    virtual void doMakeResident(Deserializer& deserializer) = 0;

private:
    bool m_isResident;
};

}