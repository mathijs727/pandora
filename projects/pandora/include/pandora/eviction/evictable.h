#pragma once
#include "pandora/eviction/fifo_cache.h"
#include <memory>

namespace pandora
{

using EvictableResourceID = uint32_t;

template <typename T>
class EvictableResourceHandle
{
public:
    EvictableResourceHandle(const FifoCache<T>& cache, EvictableResourceID resource);

    std::shared_ptr<T> getBlocking() const;
private:
    FifoCache<T> m_cache;
    EvictableResourceID m_resourceID;
};

template<typename T>
inline EvictableResourceHandle<T>::EvictableResourceHandle(const FifoCache<T>& cache, EvictableResourceID resourceID) :
    m_cache(cache), m_resourceID(resourceID)
{
}

template<typename T>
inline std::shared_ptr<T> EvictableResourceHandle<T>::getBlocking() const
{
    auto* mutThisPtr = const_cast<EvictableResourceHandle<T>*>(this);
    return mutThisPtr->m_cache->accessResource(m_resourceID, callback);
}

}