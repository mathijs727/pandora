#pragma once
#include "stream/cache/cache.h"
#include "stream/cache/cached_ptr.h"
#include "stream/cache/evictable.h"
#include <atomic>

namespace stream {

class DummyCache {
public:
    class Builder;

    template <typename T>
    CachedPtr<T> makeResident(T* pEvictable);

private:
    DummyCache();

private:
    std::atomic_uint32_t m_dummyRefCounter { 0 };
};

class DummyCache::Builder : CacheBuilder {
public:
    void registerCacheable(Evictable* pItem);

    DummyCache build();
};

template <typename T>
inline CachedPtr<T> DummyCache::makeResident(T* pEvictable)
{
    return CachedPtr<T>(pEvictable, &m_dummyRefCounter);
}

}