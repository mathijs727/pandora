#pragma once
#include "pandora/config.h"
#include <memory>

namespace pandora {

using EvictableResourceID = uint32_t;

template <typename T, typename Cache>
class EvictableResourceHandle {
public:
    EvictableResourceHandle(Cache* cache, EvictableResourceID resource);
    EvictableResourceHandle(const EvictableResourceHandle<T, Cache>&) = default;

    bool inCache() const;
    std::shared_ptr<T> getBlocking() const;

private:
    Cache* m_cache;
    EvictableResourceID m_resourceID;
};

template <typename T, typename Cache>
inline EvictableResourceHandle<T, Cache>::EvictableResourceHandle(
    Cache* cache,
    EvictableResourceID resourceID)
    : m_cache(cache)
    , m_resourceID(resourceID)
{
}

template <typename T, typename Cache>
inline bool EvictableResourceHandle<T, Cache>::inCache() const
{
    return m_cache->inCache(m_resourceID);
}

template <typename T, typename Cache>
inline std::shared_ptr<T> EvictableResourceHandle<T, Cache>::getBlocking() const
{
    auto* mutThisPtr = const_cast<EvictableResourceHandle<T, Cache>*>(this);
    return mutThisPtr->m_cache->getBlocking<T>(m_resourceID);
}

}
