#pragma once
#include "pandora/geometry/shape.h"
#include "pandora/math/vec3.h"
#include <string_view>
#include <vector>
#include <gsl/span>

namespace pandora {

class TriangleMesh : public Shape {
public:
    TriangleMesh(
        std::vector<Vec3i>&& indices,
        std::vector<Vec3f>&& positions,
        std::vector<Vec3f>&& normals);
    virtual ~TriangleMesh() = default;

    static std::unique_ptr<TriangleMesh> singleTriangle();
    static std::unique_ptr<TriangleMesh> loadFromFile(const std::string_view filename);

    unsigned numPrimitives() const override;
    /*gsl::span<const Bounds3f> getPrimitivesBounds() const override;

    //unsigned addToEmbreeScene(RTCScene& scene) const override;
    bool intersect(unsigned primitiveIndex, Ray& ray) const override;
    bool intersectMollerTrumbore(unsigned primitiveIndex, Ray& ray) const;
    bool intersectPbrt(unsigned primitiveIndex, Ray& ray) const;

    Vec3f getNormal(unsigned primitiveIndex, Vec2f uv) const override;*/

	const gsl::span<const Vec3i> getIndices() const;
	const gsl::span<const Vec3f> getPositions() const;
	const gsl::span<const Vec3f> getNormals() const;
private:
    unsigned m_numPrimitives;

    std::vector<Bounds3f> m_primitiveBounds;
    std::vector<Vec3i> m_indices;
    std::vector<Vec3f> m_positions;
    std::vector<Vec3f> m_normals;
};
}