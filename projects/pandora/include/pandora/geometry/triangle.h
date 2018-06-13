#pragma once
#include "glm/glm.hpp"
#include "pandora/geometry/bounds.h"
#include "pandora/shading/material.h"
#include <gsl/span>
#include <string_view>
#include <tuple>
#include <vector>

namespace pandora {

class TriangleMesh {
public:
    TriangleMesh(
        std::vector<glm::ivec3>&& indices,
        std::vector<glm::vec3>&& positions,
        std::vector<glm::vec3>&& normals);
    ~TriangleMesh() = default;

    static std::vector<std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>>> loadFromFile(const std::string_view filename, glm::mat4 transform = glm::mat4(1));

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
