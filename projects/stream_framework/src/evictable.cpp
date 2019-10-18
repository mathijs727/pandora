#include "stream/evictable.h"

namespace stream {

Evictable::Evictable(bool resident)
    : m_isResident(resident)
{
}

void Evictable::makeResident()
{
    doMakeResident();
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