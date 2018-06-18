#pragma once
#include "glm/glm.hpp"
#include "pandora/core/interaction.h"
#include "pandora/core/material.h"
#include "pandora/geometry/bounds.h"
#include <gsl/span>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>

struct aiScene;

namespace pandora {

class TriangleMesh {
public:
    // Public constructor because std::make_shared does not work on private constructors
    TriangleMesh(unsigned numTriangles,
        unsigned numVertices,
        std::unique_ptr<glm::ivec3[]>&& triangles,
        std::unique_ptr<glm::vec3[]>&& positions,
        std::unique_ptr<glm::vec3[]>&& normals,
        std::unique_ptr<glm::vec3[]>&& tangents,
        std::unique_ptr<glm::vec2[]>&& uvCoords);
    ~TriangleMesh() = default;

    static std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>> createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& transform);
    static std::vector<std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>>> loadFromFile(const std::string_view filename, glm::mat4 transform = glm::mat4(1));

    unsigned numTriangles() const;
    unsigned numVertices() const;

    gsl::span<const glm::ivec3> getTriangles() const;
    gsl::span<const glm::vec3> getPositions() const;

    // Temporary, to work with native Embree intersection kernels.
    // TODO: Replace this (and the getters above) with a primitive abstraction (bounds + split + intersect) so the Embree back-end uses the same intersection code as my own (TODO) kernels.
    // This abstraction should be higher performance than the shared_ptr for each triangle approach that PBRTv3 takes.
    SurfaceInteraction partialFillSurfaceInteraction(unsigned primID, const glm::vec2& hitUV) const;// Caller should initialize sceneObject & wo

    float primitiveArea(unsigned primitiveID) const;
    std::pair<Interaction, float> samplePrimitive(unsigned primitiveID, const glm::vec2& randomSample) const;
    std::pair<Interaction, float> samplePrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec2& randomSample) const;
private:
    void getUVs(unsigned primitiveID, gsl::span<glm::vec2, 3> uv) const;
    void getPs(unsigned primitiveID, gsl::span<glm::vec3, 3> p) const;
private:
    const unsigned m_numTriangles, m_numVertices;

    std::unique_ptr<glm::ivec3[]> m_triangles;
    const std::unique_ptr<glm::vec3[]> m_positions;
    const std::unique_ptr<glm::vec3[]> m_normals;
    const std::unique_ptr<glm::vec3[]> m_tangents;
    const std::unique_ptr<glm::vec2[]> m_uvCoords;
};
}
