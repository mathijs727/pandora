#pragma once
#include "pandora/math/bounds3.h"
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
#include <gsl/gsl>
#include <memory>
#include <string_view>

namespace pandora {

class TriangleMesh {
public:
    struct Triangle {
        unsigned i0, i1, i2;
    };

public:
    TriangleMesh(
        std::unique_ptr<Triangle[]>&& indices,
        std::unique_ptr<Vec3f[]>&& positions,
        std::unique_ptr<Vec3f[]>&& normals,
        unsigned numPrimitives);

    static void loadFromFile(const std::string_view filename);

    gsl::span<const Bounds3f> getPrimitivesBounds();

    Bounds3f getPrimitiveBounds(unsigned primitiveIndex);
    Vec3f getNormal(unsigned primitiveIndex, Vec2f uv);

private:
    unsigned m_numPrimitives;
    std::unique_ptr<Triangle[]> m_indices;
    std::unique_ptr<Bounds3f[]> m_primitiveBounds;
    std::unique_ptr<Vec3f[]> m_positions;
    std::unique_ptr<Vec3f[]> m_normals;
};
}