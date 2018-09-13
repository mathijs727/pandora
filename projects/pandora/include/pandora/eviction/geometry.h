#pragma once
#include "pandora/eviction/evictable.h"
#include "pandora/geometry/triangle.h"
#include <string>
#include <string_view>

namespace pandora {

/*class EvictableGeometry : public EvictableResource {
public:
    static EvictableGeometry createFromMeshFile(TriangleMesh&& mesh, std::string_view cacheFilename);
    static EvictableGeometry createFromCacheFile(std::string_view filename);

    void shrink() override final;
    void expand() override final;
    bool isShrunk() const override final;

    size_t byteSize() const;

    const TriangleMesh& get() const;

private:
    EvictableGeometry(std::string_view cacheFilename);

private:
    std::string m_cacheFilename;
    std::optional<TriangleMesh> m_meshOpt;
};*/

}
