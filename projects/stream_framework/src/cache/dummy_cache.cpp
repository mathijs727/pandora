#include "stream/cache/dummy_cache.h"

namespace stream {

void DummyCache::Builder::registerCacheable(Evictable* pItem, bool evict)
{
    if (evict)
        pItem->evict();
}
DummyCache DummyCache::Builder::build()
{
    return DummyCache();
}

}