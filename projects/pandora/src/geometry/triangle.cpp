#include "pandora/geometry/triangle.h"
#include <fstream>

namespace pandora {

TriangleMesh::TriangleMesh(
    std::unique_ptr<Triangle[]>&& indices,
    std::unique_ptr<Vec3f[]>&& positions,
    std::unique_ptr<Vec3f[]>&& normals,
    unsigned numPrimitives)
    : m_numPrimitives(numPrimitives)
    , m_indices(std::move(indices))
    , m_primitiveBounds(new Bounds3f[numPrimitives])
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
{
    for (unsigned i = 0; i < numPrimitives; i++) {
        Triangle indices = m_indices[i];
        Bounds3f& bounds = m_primitiveBounds[i];
        bounds.reset();
        bounds.grow(m_positions[indices.i0]);
        bounds.grow(m_positions[indices.i1]);
        bounds.grow(m_positions[indices.i2]);
    }
}

static bool fileExists(const std::string_view name)
{
    std::ifstream f(name.data());
    return f.good() && f.is_open();
}

void TriangleMesh::loadFromFile(const std::string_view filename)
{
    //TODO(Mathijs): move this out of the triangle class because it should also load material information
    // which will be stored in a SceneObject kinda way.
}

gsl::span<const Bounds3f> TriangleMesh::getPrimitivesBounds()
{
    return gsl::span<const Bounds3f>(m_primitiveBounds, m_numPrimitives);
}

Bounds3f TriangleMesh::getPrimitiveBounds(unsigned primitiveIndex)
{
    return m_primitiveBounds[primitiveIndex];
}

Vec3f TriangleMesh::getNormal(unsigned primitiveIndex, Vec2f uv)
{
    return m_normals[primitiveIndex];
}
}