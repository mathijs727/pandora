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
    EvictableResourceHandle(FifoCache<T>& cache, EvictableResourceID resource);

    template <typename F>
    void lock(F&& callback) const;
private:
    FifoCache<T>& m_cache;
    EvictableResourceID m_resourceID;
    std::shared_ptr<T> m_resource;
};

template<typename T>
inline EvictableResourceHandle<T>::EvictableResourceHandle(FifoCache<T>& cache, EvictableResourceID resourceID) :
    m_cache(cache), m_resourceID(resourceID)
{
}

template<typename T>
template <typename F>
inline void EvictableResourceHandle<T>::lock(F&& callback) const
{
    auto* mutThisPtr = const_cast<EvictableResourceHandle<T>*>(this);
    return mutThisPtr->m_cache.accessResource(m_resourceID, callback);
}

}