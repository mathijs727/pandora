#pragma once
#include "stream/cache/cache.h"
#include "stream/cache/cached_ptr.h"
#include "stream/cache/evictable.h"
#include <atomic>

namespace tasking {

class DummyCache {
public:
    class Builder;

    template <typename T>
    CachedPtr<T> makeResident(T* pEvictable, bool evict = false);

private:
    DummyCache() = default;
};

class DummyCache::Builder : public CacheBuilder {
public:
    void registerCacheable(Evictable* pItem, bool evict = false) final;

    DummyCache build();
};

template <typename T>
inline CachedPtr<T> DummyCache::makeResident(T* pEvictable, bool)
{
    return CachedPtr<T>(pEvictable, nullptr);
}

}