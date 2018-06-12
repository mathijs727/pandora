#pragma once
#include "glm/glm.hpp"
#include "pandora/geometry/bounds.h"
#include <gsl/span>
#include <string_view>
#include <vector>

namespace pandora {

class TriangleMesh {
public:
    TriangleMesh(
        std::vector<glm::ivec3>&& indices,
        std::vector<glm::vec3>&& positions,
        std::vector<glm::vec3>&& normals);
    ~TriangleMesh() = default;

    static std::shared_ptr<const TriangleMesh> singleTriangle();
    static std::shared_ptr<const TriangleMesh> loadFromFile(const std::string_view filename);

    unsigned numPrimitives() const;

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
