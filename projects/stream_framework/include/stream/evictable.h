#pragma once

namespace stream {

class Evictable {
public:
    Evictable(bool resident);
    virtual ~Evictable() {};

    virtual size_t sizeBytes() const = 0;

    void evict();
    void makeResident();

    bool isResident() const noexcept;

protected:
    virtual void doEvict() = 0;
    virtual void doMakeResident() = 0;

private:
    bool m_isResident;
};

}