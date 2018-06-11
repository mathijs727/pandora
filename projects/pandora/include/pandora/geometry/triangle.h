#pragma once
#include "glm/glm.hpp"
#include "pandora/geometry/shape.h"
#include <gsl/span>
#include <string_view>
#include <vector>

namespace pandora {

class TriangleMesh : public Shape {
public:
    TriangleMesh(
        std::vector<glm::ivec3>&& indices,
        std::vector<glm::vec3>&& positions,
        std::vector<glm::vec3>&& normals);
    virtual ~TriangleMesh() = default;

    static std::unique_ptr<TriangleMesh> singleTriangle();
    static std::unique_ptr<TriangleMesh> loadFromFile(const std::string_view filename);

    unsigned numPrimitives() const override;
    /*gsl::span<const Bounds> getPrimitivesBounds() const override;

    //unsigned addToEmbreeScene(RTCScene& scene) const override;
    bool intersect(unsigned primitiveIndex, Ray& ray) const override;
    bool intersectMollerTrumbore(unsigned primitiveIndex, Ray& ray) const;
    bool intersectPbrt(unsigned primitiveIndex, Ray& ray) const;

    glm::vec3 getNormal(unsigned primitiveIndex, glm::vec2 uv) const override;*/

	const gsl::span<const glm::ivec3> getIndices() const;
	const gsl::span<const glm::vec3> getPositions() const;
	const gsl::span<const glm::vec3> getNormals() const;
private:
    unsigned m_numPrimitives;

    std::vector<Bounds> m_primitiveBounds;
    std::vector<glm::ivec3> m_indices;
    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_normals;
};
}
