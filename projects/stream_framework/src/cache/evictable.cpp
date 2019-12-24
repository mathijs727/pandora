#include "stream/cache/evictable.h"

namespace tasking {

Evictable::Evictable(bool resident)
    : m_isResident(resident)
{
}

void Evictable::makeResident(Deserializer& deserializer)
{
    doMakeResident(deserializer);
    m_isResident = true;
}

void Evictable::evict()
{
    doEvict();
    m_isResident = false;
}

bool Evictable::isResident() const noexcept
{
    return m_isResident;
}

}