#pragma once
#include "stream/cache/evictable.h"
#include "stream/serialize/serializer.h"

namespace stream {

class CacheBuilder {
public:
    virtual void registerCacheable(Evictable* pItem) = 0;
};

}