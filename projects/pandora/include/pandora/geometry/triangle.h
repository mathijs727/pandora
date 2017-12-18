#pragma once
#include "pandora/math/bounds3.h"
#include "pandora/math/vec2.h"
#include "pandora/math/vec3.h"
#include "pandora/traversal/ray.h"
#include <gsl/gsl>
#include <string_view>
#include <vector>

namespace pandora {

class TriangleMesh {
public:
    struct Triangle {
        unsigned i0, i1, i2;
    };

public:
    TriangleMesh(
        std::vector<Triangle>&& indices,
        std::vector<Vec3f>&& positions,
        std::vector<Vec3f>&& normals);

    static std::unique_ptr<TriangleMesh> singleTriangle();
    static std::unique_ptr<TriangleMesh> loadFromFile(const std::string_view filename);

    unsigned numPrimitives();
    gsl::span<const Bounds3f> getPrimitivesBounds();

    Bounds3f getPrimitiveBounds(unsigned primitiveIndex);
    Vec3f getNormal(unsigned primitiveIndex, Vec2f uv);

    bool intersect(unsigned primitiveIndex, Ray& ray);

private:
    unsigned m_numPrimitives;

    std::vector<Bounds3f> m_primitiveBounds;
    std::vector<Triangle> m_indices;
    std::vector<Vec3f> m_positions;
    std::vector<Vec3f> m_normals;
};
}