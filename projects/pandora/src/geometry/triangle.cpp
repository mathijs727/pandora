#include "pandora/geometry/triangle.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/mat4x4.hpp"
#include "pandora/core/stats.h"
#include "pandora/core/transform.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <mio/mmap.hpp>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>

using namespace std::string_literals;

static bool fileExists(const std::string_view name)
{
    std::ifstream f(name.data());
    return f.good() && f.is_open();
}

static glm::mat4 assimpMatrix(const aiMatrix4x4& m)
{
    //float values[3][4] = {};
    glm::mat4 matrix;
    matrix[0][0] = m.a1;
    matrix[0][1] = m.b1;
    matrix[0][2] = m.c1;
    matrix[0][3] = m.d1;
    matrix[1][0] = m.a2;
    matrix[1][1] = m.b2;
    matrix[1][2] = m.c2;
    matrix[1][3] = m.d2;
    matrix[2][0] = m.a3;
    matrix[2][1] = m.b3;
    matrix[2][2] = m.c3;
    matrix[2][3] = m.d3;
    matrix[3][0] = m.a4;
    matrix[3][1] = m.b4;
    matrix[3][2] = m.c4;
    matrix[3][3] = m.d4;
    return matrix;
}

static glm::vec3 assimpVec(const aiVector3D& v)
{
    return glm::vec3(v.x, v.y, v.z);
}

namespace pandora {

TriangleMesh::TriangleMesh(
    unsigned numTriangles,
    unsigned numVertices,
    std::unique_ptr<glm::ivec3[]>&& triangles,
    std::unique_ptr<glm::vec3[]>&& positions,
    std::unique_ptr<glm::vec3[]>&& normals,
    std::unique_ptr<glm::vec3[]>&& tangents,
    std::unique_ptr<glm::vec2[]>&& uvCoords)
    : m_numTriangles(numTriangles)
    , m_numVertices(numVertices)
    , m_triangles(std::move(triangles))
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
    , m_tangents(std::move(tangents))
    , m_uvCoords(std::move(uvCoords))
{
    for (unsigned v = 0; v < numVertices; v++)
        m_bounds.grow(m_positions[v]);

    g_stats.memory.geometryLoaded += sizeBytes();
}

TriangleMesh::TriangleMesh(const serialization::TriangleMesh* serializedTriangleMesh)
{
    m_numTriangles = serializedTriangleMesh->numTriangles();
    m_numVertices = serializedTriangleMesh->numVertices();

    m_triangles = std::make_unique<glm::ivec3[]>(m_numTriangles);
    std::memcpy(m_triangles.get(), serializedTriangleMesh->triangles()->data(), serializedTriangleMesh->triangles()->size());

    m_positions = std::make_unique<glm::vec3[]>(m_numVertices);
    std::memcpy(m_positions.get(), serializedTriangleMesh->positions()->data(), serializedTriangleMesh->positions()->size());

    if (serializedTriangleMesh->normals()) {
        m_normals = std::make_unique<glm::vec3[]>(m_numVertices);
        std::memcpy(m_normals.get(), serializedTriangleMesh->normals()->data(), serializedTriangleMesh->normals()->size());
    }

    if (serializedTriangleMesh->tangents()) {
        m_tangents = std::make_unique<glm::vec3[]>(m_numVertices);
        std::memcpy(m_tangents.get(), serializedTriangleMesh->tangents()->data(), serializedTriangleMesh->tangents()->size());
    }

    if (serializedTriangleMesh->uvCoords()) {
        m_uvCoords = std::make_unique<glm::vec2[]>(m_numVertices);
        std::memcpy(m_uvCoords.get(), serializedTriangleMesh->uvCoords()->data(), serializedTriangleMesh->uvCoords()->size());
    }

    g_stats.memory.geometryLoaded += sizeBytes();
}

TriangleMesh::~TriangleMesh()
{
    g_stats.memory.geometryEvicted += sizeBytes();
}

flatbuffers::Offset<serialization::TriangleMesh> TriangleMesh::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    size_t estimatedSize = 1024 + m_numTriangles * sizeof(glm::ivec3) + m_numVertices * sizeof(glm::vec3);
    if (m_normals)
        estimatedSize += m_numVertices * sizeof(glm::vec3);
    if (m_tangents)
        estimatedSize += m_numVertices * sizeof(glm::vec3);
    if (m_uvCoords)
        estimatedSize += m_numVertices * sizeof(glm::vec2);

    auto triangles = builder.CreateVector(reinterpret_cast<const int8_t*>(m_triangles.get()), m_numTriangles * sizeof(glm::ivec3));
    auto positions = builder.CreateVector(reinterpret_cast<const int8_t*>(m_positions.get()), m_numVertices * sizeof(glm::vec3));
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> normals = 0;
    if (m_normals)
        normals = builder.CreateVector(reinterpret_cast<const int8_t*>(m_normals.get()), m_numVertices * sizeof(glm::vec3));
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> tangents = 0;
    if (m_tangents)
        tangents = builder.CreateVector(reinterpret_cast<const int8_t*>(m_tangents.get()), m_numVertices * sizeof(glm::vec3));
    flatbuffers::Offset<flatbuffers::Vector<int8_t>> uvCoords = 0;
    if (m_uvCoords)
        uvCoords = builder.CreateVector(reinterpret_cast<const int8_t*>(m_uvCoords.get()), m_numVertices * sizeof(glm::vec2));

    return serialization::CreateTriangleMesh(
        builder,
        m_numTriangles,
        m_numVertices,
        triangles,
        positions,
        normals,
        tangents,
        uvCoords);
}

TriangleMesh TriangleMesh::subMesh(gsl::span<const unsigned> primitives) const
{
    std::vector<bool> usedVertices(numVertices());
    std::fill(std::begin(usedVertices), std::end(usedVertices), false);
    for (const unsigned primitiveID : primitives) {
        const auto& triangle = m_triangles[primitiveID];
        usedVertices[triangle[0]] = true;
        usedVertices[triangle[1]] = true;
        usedVertices[triangle[2]] = true;
    }

    std::unordered_map<int, int> vertexIndexMapping;
    int numUsedVertices = 0;
    for (int i = 0; i < static_cast<int>(usedVertices.size()); i++) {
        if (usedVertices[i]) {
            vertexIndexMapping[i] = numUsedVertices++;
        }
    }

    std::unique_ptr<glm::ivec3[]> triangles = std::make_unique<glm::ivec3[]>(primitives.size());
    unsigned currentTriangle = 0;
    for (unsigned triangleIndex : primitives) {
        glm::ivec3 originalTriangle = m_triangles[triangleIndex];
        glm::ivec3 triangle = {
            vertexIndexMapping[originalTriangle[0]],
            vertexIndexMapping[originalTriangle[1]],
            vertexIndexMapping[originalTriangle[2]]
        };
        triangles[currentTriangle++] = triangle;
    }

    std::unique_ptr<glm::vec3[]> positions = std::make_unique<glm::vec3[]>(numUsedVertices);
    std::unique_ptr<glm::vec3[]> normals = m_normals ? std::make_unique<glm::vec3[]>(numUsedVertices) : nullptr;
    std::unique_ptr<glm::vec3[]> tangents = m_tangents ? std::make_unique<glm::vec3[]>(numUsedVertices) : nullptr;
    std::unique_ptr<glm::vec2[]> uvCoords = m_uvCoords ? std::make_unique<glm::vec2[]>(numUsedVertices) : nullptr;
    unsigned currentVertex = 0;
    for (unsigned vertexIndex = 0; vertexIndex < numVertices(); vertexIndex++) {
        if (usedVertices[vertexIndex]) {
            positions[currentVertex] = m_positions[vertexIndex];
            if (normals)
                normals[currentVertex] = m_normals[vertexIndex];
            if (tangents)
                tangents[currentVertex] = m_tangents[vertexIndex];
            if (uvCoords)
                uvCoords[currentVertex] = m_uvCoords[vertexIndex];
            currentVertex++;
        }
    }
    assert(currentVertex <= currentTriangle * 3);

    return TriangleMesh(currentTriangle, currentVertex, std::move(triangles), std::move(positions), std::move(normals), std::move(tangents), std::move(uvCoords));
}

TriangleMesh TriangleMesh::createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& matrix, bool ignoreVertexNormals)
{
    const aiMesh* mesh = scene->mMeshes[meshIndex];

    Transform transform(matrix);

    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
        THROW_ERROR("Empty mesh");

    auto indices = std::make_unique<glm::ivec3[]>(mesh->mNumFaces);
    auto positions = std::make_unique<glm::vec3[]>(mesh->mNumVertices);
    std::unique_ptr<glm::vec3[]> normals = nullptr; // Shading normals
    std::unique_ptr<glm::vec3[]> tangents = nullptr;
    std::unique_ptr<glm::vec2[]> uvCoords = nullptr;

    // Triangles
    for (unsigned i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            THROW_ERROR("Found a face which is not a triangle, discarding!");
        }

        auto aiIndices = face.mIndices;
        indices[i] = { aiIndices[0], aiIndices[1], aiIndices[2] };
    }

    // Positions
    for (unsigned i = 0; i < mesh->mNumVertices; i++) {
        positions[i] = transform.transformPoint(assimpVec(mesh->mVertices[i]));
    }

    // Normals
    if (mesh->HasNormals() && !ignoreVertexNormals) {
        normals = std::make_unique<glm::vec3[]>(mesh->mNumVertices);
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            normals[i] = transform.transformNormal(assimpVec(mesh->mNormals[i]));
        }
    }

    /*// UV mapping
    if (mesh->HasTextureCoords(0)) {
        uvCoords = std::make_unique<glm::vec2[]>(mesh->mNumVertices);
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            uvCoords[i] = glm::vec2(assimpVec(mesh->mTextureCoords[0][i]));
        }
    }*/

    return TriangleMesh(
        mesh->mNumFaces,
        mesh->mNumVertices,
        std::move(indices),
        std::move(positions),
        std::move(normals),
        std::move(tangents),
        std::move(uvCoords));
}

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(const std::string_view filename, size_t start, size_t length, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!fileExists(filename)) {
        LOG_WARNING("Could not find mesh file: "s + std::string(filename));
        return {};
    }

    auto mmapFile = mio::mmap_source(filename, start, length);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(mmapFile.data(), length,
        aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals,
        "obj");

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + std::string(filename));
        return {};
    }

    return loadFromFileSingleMesh(scene, objTransform, ignoreVertexNormals);
}

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(const std::string_view filename, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!fileExists(filename)) {
        LOG_WARNING("Could not find mesh file: "s + std::string(filename));
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.data(),
        aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + std::string(filename));
        return {};
    }

    return loadFromFileSingleMesh(scene, objTransform, ignoreVertexNormals);
}

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(const aiScene* scene, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    std::vector<glm::ivec3> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;

    std::stack<std::tuple<aiNode*, glm::mat4>> stack;
    stack.push({ scene->mRootNode, objTransform * assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto [node, matrix] = stack.top();
        stack.pop();

        matrix *= assimpMatrix(node->mTransformation);
        Transform transform(matrix);

        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            // Process subMesh
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

            if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
                THROW_ERROR("Empty mesh");

            // Triangles
            auto indexOffset = positions.size();
            for (unsigned j = 0; j < mesh->mNumFaces; j++) {
                const aiFace& face = mesh->mFaces[j];
                if (face.mNumIndices != 3) {
                    THROW_ERROR("Found a face which is not a triangle, discarding!");
                }

                auto aiIndices = face.mIndices;
                indices.push_back(glm::ivec3 {
                    aiIndices[0] + indexOffset,
                    aiIndices[1] + indexOffset,
                    aiIndices[2] + indexOffset });
            }

            // Positions
            for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                glm::vec3 pos = assimpVec(mesh->mVertices[j]);
                glm::vec3 transformedPos = transform.transformPoint(pos);
                positions.push_back(transformedPos);
            }

            // Normals
            if (mesh->HasNormals() && !ignoreVertexNormals) {
                for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                    normals.push_back(transform.transformNormal(assimpVec(mesh->mNormals[j])));
                }
            } else {
                std::cout << "WARNING: submesh has no normal vectors" << std::endl;
                for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                    normals.push_back(glm::vec3(0));
                }
            }
        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], matrix });
        }
    }

    ALWAYS_ASSERT(positions.size() == normals.size());
    std::unique_ptr<glm::ivec3[]> pIndices = std::make_unique<glm::ivec3[]>(indices.size());
    std::copy(std::begin(indices), std::end(indices), pIndices.get());
    std::unique_ptr<glm::vec3[]> pPositions = std::make_unique<glm::vec3[]>(positions.size());
    std::copy(std::begin(positions), std::end(positions), pPositions.get());
    std::unique_ptr<glm::vec3[]> pNormals = std::make_unique<glm::vec3[]>(normals.size());
    std::copy(std::begin(normals), std::end(normals), pNormals.get());
    std::unique_ptr<glm::vec3[]> pTangents = nullptr;
    std::unique_ptr<glm::vec2[]> pUvCoords = nullptr;

    return TriangleMesh(
        static_cast<unsigned>(indices.size()),
        static_cast<unsigned>(positions.size()),
        std::move(pIndices),
        std::move(pPositions),
        std::move(pNormals),
        std::move(pTangents),
        std::move(pUvCoords));
}

std::vector<TriangleMesh> TriangleMesh::loadFromFile(const std::string_view filename, glm::mat4 modelTransform, bool ignoreVertexNormals)
{
    if (!fileExists(filename)) {
        LOG_WARNING("Could not find mesh file: "s + std::string(filename));
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.data(),
        aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);
    //importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + std::string(filename));
        return {};
    }

    std::vector<TriangleMesh> result;

    std::stack<std::tuple<aiNode*, glm::mat4>> stack;
    stack.push({ scene->mRootNode, modelTransform * assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto [node, transform] = stack.top();
        stack.pop();

        transform *= assimpMatrix(node->mTransformation);

        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            // Process subMesh
            result.push_back(createMeshAssimp(scene, node->mMeshes[i], transform, ignoreVertexNormals));
        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], transform });
        }
    }

    return result;
}

size_t TriangleMesh::sizeBytes() const
{
    size_t size = sizeof(TriangleMesh);
    size += m_numTriangles * sizeof(glm::ivec3); // triangles
    size += m_numVertices * sizeof(glm::vec3); // positions
    if (m_normals)
        size += m_numVertices * sizeof(glm::vec3); // normals
    if (m_tangents)
        size += m_numVertices * sizeof(glm::vec3); // tangents
    if (m_uvCoords)
        size += m_numVertices * sizeof(glm::vec2); // uv coords
    return size;
}

unsigned TriangleMesh::numTriangles() const
{
    return m_numTriangles;
}

unsigned TriangleMesh::numVertices() const
{
    return m_numVertices;
}

gsl::span<const glm::ivec3> TriangleMesh::getTriangles() const
{
    return gsl::make_span(m_triangles.get(), m_numTriangles);
}

gsl::span<const glm::vec3> TriangleMesh::getPositions() const
{
    return gsl::make_span(m_positions.get(), m_numVertices);
}

gsl::span<const glm::vec3> TriangleMesh::getNormals() const
{
    return gsl::make_span(m_normals.get(), m_numVertices);
}

gsl::span<const glm::vec3> TriangleMesh::getTangents() const
{
    return gsl::make_span(m_tangents.get(), m_numVertices);
}

gsl::span<const glm::vec2> TriangleMesh::getUVCoords() const
{
    return gsl::make_span(m_uvCoords.get(), m_numVertices);
}

Bounds TriangleMesh::getBounds() const
{
    return m_bounds;
}

Bounds TriangleMesh::getPrimitiveBounds(unsigned primitiveID) const
{
    glm::ivec3 t = m_triangles[primitiveID];

    Bounds bounds;
    bounds.grow(m_positions[t[0]]);
    bounds.grow(m_positions[t[1]]);
    bounds.grow(m_positions[t[2]]);
    return bounds;
}

float TriangleMesh::primitiveArea(unsigned primitiveID) const
{
    const auto& triangle = m_triangles[primitiveID];
    const glm::vec3& p0 = m_positions[triangle[0]];
    const glm::vec3& p1 = m_positions[triangle[1]];
    const glm::vec3& p2 = m_positions[triangle[2]];
    return 0.5f * glm::length(glm::cross(p1 - p0, p2 - p0));
}

// PBRTv3 page 839
Interaction TriangleMesh::samplePrimitive(unsigned primitiveID, const glm::vec2& randomSample) const
{
    // Compute uniformly sampled barycentric coordinates
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    float su0 = std::sqrt(randomSample[0]);
    glm::vec2 b = glm::vec2(1 - su0, randomSample[1] * su0);

    const auto& triangle = m_triangles[primitiveID];
    const glm::vec3& p0 = m_positions[triangle[0]];
    const glm::vec3& p1 = m_positions[triangle[1]];
    const glm::vec3& p2 = m_positions[triangle[2]];

    Interaction it;
    it.position = b[0] * p0 + b[1] * p1 + (1 - b[0] - b[1]) * p2;
    it.normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    return it;
}

// PBRTv3 page 837
Interaction TriangleMesh::samplePrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec2& randomSample) const
{
    (void)ref;
    return samplePrimitive(primitiveID, randomSample);
}

// PBRTv3 page 837
float TriangleMesh::pdfPrimitive(unsigned primitiveID, const Interaction& ref) const
{
    (void)ref;
    return 1.0f / primitiveArea(primitiveID);
}

// PBRTv3 page 837
float TriangleMesh::pdfPrimitive(unsigned primitiveID, const Interaction& ref, const glm::vec3& wi) const
{
    if (glm::dot(ref.normal, wi) <= 0.0f)
        return 0.0f;

    // Intersect sample ray with area light geometry
    Ray ray = ref.spawnRay(wi);
    RayHit hitInfo;
    if (!intersectPrimitive(ray, hitInfo, primitiveID, false))
        return 0.0f;

    SurfaceInteraction isectLight = fillSurfaceInteraction(ray, hitInfo, false);

    // Convert light sample weight to solid angle measure
    return distanceSquared(ref.position, isectLight.position) / (absDot(isectLight.normal, -wi) * primitiveArea(primitiveID));
}

void TriangleMesh::getUVs(unsigned primitiveID, gsl::span<glm::vec2, 3> uv) const
{
    if (m_uvCoords) {
        glm::ivec3 indices = m_triangles[primitiveID];
        uv[0] = m_uvCoords[indices[0]];
        uv[1] = m_uvCoords[indices[1]];
        uv[2] = m_uvCoords[indices[2]];
    } else {
        uv[0] = glm::vec2(0, 0);
        uv[1] = glm::vec2(1, 0);
        uv[2] = glm::vec2(1, 1);
    }
}

void TriangleMesh::getPs(unsigned primitiveID, gsl::span<glm::vec3, 3> p) const
{
    glm::ivec3 indices = m_triangles[primitiveID];
    p[0] = m_positions[indices[0]];
    p[1] = m_positions[indices[1]];
    p[2] = m_positions[indices[2]];
}

void TriangleMesh::voxelize(VoxelGrid& grid, const Bounds& gridBounds, const Transform& transform) const
{
    // Map world space to [0, 1]
    float scale = maxComponent(gridBounds.extent());
    glm::vec3 offset = gridBounds.min;
    glm::ivec3 gridResolution = glm::ivec3(grid.resolution());

    // World space extent of a voxel
    glm::vec3 delta_p = scale / glm::vec3(grid.resolution());

    // Helper functions
    glm::vec3 worldToVoxelScale = glm::vec3(grid.resolution()) / scale;
    glm::vec3 voxelToWorldScale = scale / glm::vec3(grid.resolution());
    const auto worldToVoxel = [&](const glm::vec3& worldVec) -> glm::ivec3 { return glm::ivec3((worldVec - offset) * worldToVoxelScale); };
    const auto voxelToWorld = [&](const glm::ivec3& voxel) -> glm::vec3 { return glm::vec3(voxel) * voxelToWorldScale + offset; };

    const glm::ivec3 maxGridVoxel(grid.resolution() - 1);

    for (unsigned t = 0; t < m_numTriangles; t++) {
        const auto& triangle = m_triangles[t];
        glm::vec3 v[3] = {
            transform.transformPoint(m_positions[triangle[0]]),
            transform.transformPoint(m_positions[triangle[1]]),
            transform.transformPoint(m_positions[triangle[2]])
        };
        glm::vec3 e[3] = { v[1] - v[0], v[2] - v[1], v[0] - v[2] };
        glm::vec3 n = glm::cross(e[0], e[1]);

        // Triangle bounds
        glm::vec3 tBoundsMin = glm::min(v[0], glm::min(v[1], v[2]));
        glm::vec3 tBoundsMax = glm::max(v[0], glm::max(v[1], v[2]));
        glm::vec3 tBoundsExtent = tBoundsMax - tBoundsMin;

        glm::ivec3 tBoundsMinVoxel = glm::min(worldToVoxel(tBoundsMin), gridResolution - 1); // Fix for triangles on the border of the voxel grid
        glm::ivec3 tBoundsMaxVoxel = worldToVoxel(tBoundsMin + tBoundsExtent) + 1; // Upper bound
        glm::ivec3 tBoundsExtentVoxel = tBoundsMaxVoxel - tBoundsMinVoxel;

        if (tBoundsExtentVoxel.x == 1 && tBoundsExtentVoxel.y == 1 && tBoundsExtentVoxel.z == 1) {
            grid.set(tBoundsMinVoxel.x, tBoundsMinVoxel.y, tBoundsMinVoxel.z, true);
        } else {
            // Critical point
            glm::vec3 c(
                n.x > 0 ? delta_p.x : 0,
                n.y > 0 ? delta_p.y : 0,
                n.z > 0 ? delta_p.z : 0);
            float d1 = glm::dot(n, c - v[0]);
            float d2 = glm::dot(n, (delta_p - c) - v[0]);

            // For each voxel in the triangles AABB
            for (int z = tBoundsMinVoxel.z; z < std::min(tBoundsMaxVoxel.z, gridResolution.z); z++) {
                for (int y = tBoundsMinVoxel.y; y < std::min(tBoundsMaxVoxel.y, gridResolution.y); y++) {
                    for (int x = tBoundsMinVoxel.x; x < std::min(tBoundsMaxVoxel.x, gridResolution.x); x++) {
                        // Intersection test
                        glm::vec3 p = voxelToWorld(glm::ivec3(x, y, z));

                        bool planeIntersect = ((glm::dot(n, p) + d1) * (glm::dot(n, p) + d2)) <= 0;
                        if (!planeIntersect)
                            continue;

                        bool triangleIntersect2D = true;
                        for (int i = 0; i < 3; i++) {
                            // Test overlap between the projection of the triangle and AABB on the XY-plane
                            if (std::abs(n.z) > 0) {
                                glm::vec2 n_xy_ei = glm::vec2(-e[i].y, e[i].x) * (n.z >= 0 ? 1.0f : -1.0f);
                                glm::vec2 v_xy_i(v[i].x, v[i].y);
                                glm::vec2 p_xy_i(p.x, p.y);
                                float distFromEdge = glm::dot(p_xy_i, n_xy_ei) + std::max(0.0f, delta_p.x * n_xy_ei.x) + std::max(0.0f, delta_p.y * n_xy_ei.y) - glm::dot(n_xy_ei, v_xy_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the ZX-plane
                            if (std::abs(n.y) > 0) {
                                glm::vec2 n_xz_ei = glm::vec2(-e[i].z, e[i].x) * (n.y >= 0 ? -1.0f : 1.0f);
                                glm::vec2 v_xz_i(v[i].x, v[i].z);
                                glm::vec2 p_xz_i(p.x, p.z);
                                float distFromEdge = glm::dot(p_xz_i, n_xz_ei) + std::max(0.0f, delta_p.x * n_xz_ei.x) + std::max(0.0f, delta_p.z * n_xz_ei.y) - glm::dot(n_xz_ei, v_xz_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }

                            // Test overlap between the projection of the triangle and AABB on the YZ-plane
                            if (std::abs(n.x) > 0) {
                                glm::vec2 n_yz_ei = glm::vec2(-e[i].z, e[i].y) * (n.x >= 0 ? 1.0f : -1.0f);
                                glm::vec2 v_yz_i(v[i].y, v[i].z);
                                glm::vec2 p_yz_i(p.y, p.z);
                                float distFromEdge = glm::dot(p_yz_i, n_yz_ei) + std::max(0.0f, delta_p.y * n_yz_ei.x) + std::max(0.0f, delta_p.z * n_yz_ei.y) - glm::dot(n_yz_ei, v_yz_i);
                                triangleIntersect2D &= distFromEdge >= 0;
                            }
                        }

                        Bounds triangleBounds;
                        triangleBounds.grow(v[0]);
                        triangleBounds.grow(v[1]);
                        triangleBounds.grow(v[2]);
                        Bounds voxelBounds = Bounds(p, p + delta_p);
                        triangleIntersect2D &= triangleBounds.overlaps(voxelBounds);

                        if (triangleIntersect2D)
                            grid.set(x, y, z, true);
                    }
                }
            }
        }
    }
}

}
