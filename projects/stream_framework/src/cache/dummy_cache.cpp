#include "stream/cache/dummy_cache.h"

namespace stream {

void DummyCache::Builder::registerCacheable(Evictable* pItem)
{
}

DummyCache DummyCache::Builder::build()
{
    return DummyCache();
}

}