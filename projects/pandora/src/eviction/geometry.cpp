#include "pandora/eviction/geometry.h"
#include "pandora/core/scene.h"

namespace pandora {

EvictableGeometryHandle::EvictableGeometryHandle(FifoCache<GeometryCollection>& cache, EvictableResourceID resourceID)
    : m_cache(cache)
    , m_resourceID(resourceID)
{
}

/*EvictableGeometry EvictableGeometry::createFromMeshFile(TriangleMesh&& mesh, std::string_view cacheFilename)
{
    auto myMesh = std::move(mesh);
    myMesh.saveToCacheFile(cacheFilename);
    return EvictableGeometry(cacheFilename); // Mesh will be destroyed here because it goes out of scope
}

EvictableGeometry EvictableGeometry::createFromCacheFile(std::string_view filename)
{
    return EvictableGeometry(filename);
}

EvictableGeometry::EvictableGeometry(std::string_view cacheFilename)
    : m_cacheFilename(cacheFilename)
    , m_meshOpt({})
{
}

void EvictableGeometry::shrink()
{
    assert(m_meshOpt.has_value());
    m_meshOpt.reset();
}

void EvictableGeometry::expand()
{
    assert(!m_meshOpt.has_value());
    m_meshOpt.emplace(TriangleMesh::loadFromCacheFile(m_cacheFilename));
}
bool EvictableGeometry::isShrunk() const
{
    return m_meshOpt.has_value();
}

size_t EvictableGeometry::byteSize() const
{
    assert(m_meshOpt.has_value());
    return 0; // TODO
}

const TriangleMesh& EvictableGeometry::get() const
{
    assert(m_meshOpt.has_value());
    return *m_meshOpt;
}*/

}
