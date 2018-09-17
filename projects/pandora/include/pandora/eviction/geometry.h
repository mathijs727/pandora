#pragma once
#include "pandora/eviction/evictable.h"
#include "pandora/geometry/triangle.h"
#include <string>
#include <string_view>

namespace pandora {

using GeometryCollection = std::vector<std::shared_ptr<TriangleMesh>>;

class EvictableGeometryHandle
{
public:
    EvictableGeometryHandle(FifoCache<GeometryCollection>& cache, EvictableResourceID resource);

    template <typename F>
    void lock(F&& callback) const;
private:
    FifoCache<GeometryCollection>& m_cache;
    EvictableResourceID m_resourceID;
};

template<typename F>
inline void EvictableGeometryHandle::lock(F && callback) const
{
    m_cache.accessResource(m_resourceID, [=](std::shared_ptr<GeometryCollection> geometryCollection) {
        callback((*geometryCollection)[0]);
    });
}

}
