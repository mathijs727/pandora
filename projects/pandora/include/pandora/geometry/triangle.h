#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/math/vec3.h"
#include <string_view>
#include <vector>

namespace pandora {

class TriangleMesh : public Shape {
public:
    struct Triangle {
        unsigned i0, i1, i2;
    };

public:
    TriangleMesh(
        std::vector<Triangle>&& indices,
        std::vector<Vec3f>&& positions,
        std::vector<Vec3f>&& normals);
    virtual ~TriangleMesh() = default;

    static std::unique_ptr<TriangleMesh> singleTriangle();
    static std::unique_ptr<TriangleMesh> loadFromFile(const std::string_view filename);

    unsigned numPrimitives() const override;
    gsl::span<const Bounds3f> getPrimitivesBounds() const override;

    //unsigned addToEmbreeScene(RTCScene& scene) const override;

    bool intersect(unsigned primitiveIndex, Ray& ray) const override;
    Vec3f getNormal(unsigned primitiveIndex, Vec2f uv) const override;

private:
    unsigned m_numPrimitives;

    std::vector<Bounds3f> m_primitiveBounds;
    std::vector<Triangle> m_indices;
    std::vector<Vec3f> m_positions;
    std::vector<Vec3f> m_normals;
};
}