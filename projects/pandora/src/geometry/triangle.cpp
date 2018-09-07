#include "pandora/geometry/triangle.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/mat4x4.hpp"
#include "pandora/core/stats.h"
#include "pandora/flatbuffers/triangle_mesh_generated.h"
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

    g_stats.memory.geometry += size();
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

TriangleMesh TriangleMesh::createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& transform, bool ignoreVertexNormals)
{
    const aiMesh* mesh = scene->mMeshes[meshIndex];

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
        positions[i] = transform * glm::vec4(assimpVec(mesh->mVertices[i]), 1);
    }

    // Normals
    if (mesh->HasNormals() && !ignoreVertexNormals) {
        normals = std::make_unique<glm::vec3[]>(mesh->mNumVertices);
        glm::mat3 normalTransform = transform;
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            normals[i] = normalTransform * glm::vec3(assimpVec(mesh->mNormals[i]));
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

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(const std::string_view filename, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!fileExists(filename)) {
        LOG_WARNING("Could not find mesh file: "s + std::string(filename));
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.data(),
        aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices |
        aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);
    //importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + std::string(filename));
        return {};
    }

    std::vector<glm::ivec3> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;

    std::stack<std::tuple<aiNode*, glm::mat4>> stack;
    stack.push({ scene->mRootNode, objTransform * assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto[node, transform] = stack.top();
        stack.pop();

        transform *= assimpMatrix(node->mTransformation);

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
                indices.push_back(glm::ivec3{
                    aiIndices[0] + indexOffset,
                    aiIndices[1] + indexOffset,
                    aiIndices[2] + indexOffset });
            }

            // Positions
            for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                glm::vec3 pos = assimpVec(mesh->mVertices[j]);
                glm::vec3 transformedPos = transform * glm::vec4(pos, 1);
                positions.push_back(transformedPos);
            }

            // Normals
            if (mesh->HasNormals() && !ignoreVertexNormals) {
                glm::mat3 normalTransform = transform;
                for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                    normals.push_back(normalTransform * glm::vec3(assimpVec(mesh->mNormals[j])));
                }
            } else {
                std::cout << "WARNING: submesh has no normal vectors" << std::endl;
                for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                    normals.push_back(glm::vec3(0));
                }
            }

        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], transform });
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
        aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices |
        aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);
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

TriangleMesh TriangleMesh::loadFromCacheFile(const std::string_view filename)
{
    auto mmapFile = mio::mmap_source(filename, 0, mio::map_entire_file);
    auto triangleMesh = serialization::GetTriangleMesh(mmapFile.data());
    unsigned numTriangles = triangleMesh->numTriangles();
    unsigned numVertices = triangleMesh->numVertices();

    std::unique_ptr<glm::ivec3[]> triangles = std::make_unique<glm::ivec3[]>(numTriangles);
    std::memcpy(triangles.get(), triangleMesh->triangles()->data(), triangleMesh->triangles()->size());

    std::unique_ptr<glm::vec3[]> positions = std::make_unique<glm::vec3[]>(numVertices);
    std::memcpy(positions.get(), triangleMesh->positions()->data(), triangleMesh->positions()->size());

    std::unique_ptr<glm::vec3[]> normals;
    if (triangleMesh->normals()) {
        normals = std::make_unique<glm::vec3[]>(numVertices);
        std::memcpy(normals.get(), triangleMesh->normals()->data(), triangleMesh->normals()->size());
    }

    std::unique_ptr<glm::vec3[]> tangents;
    if (triangleMesh->tangents()) {
        tangents = std::make_unique<glm::vec3[]>(numVertices);
        std::memcpy(tangents.get(), triangleMesh->tangents()->data(), triangleMesh->tangents()->size());
    }

    std::unique_ptr<glm::vec2[]> uvCoords;
    if (triangleMesh->uvCoords()) {
        uvCoords = std::make_unique<glm::vec2[]>(numVertices);
        std::memcpy(uvCoords.get(), triangleMesh->uvCoords()->data(), triangleMesh->uvCoords()->size());
    }

    mmapFile.unmap();

    return TriangleMesh(
        numTriangles,
        numVertices,
        std::move(triangles),
        std::move(positions),
        std::move(normals),
        std::move(tangents),
        std::move(uvCoords));
}

void TriangleMesh::saveToCacheFile(const std::string_view filename)
{
    size_t estimatedSize = 1024 + m_numTriangles * sizeof(glm::ivec3) + m_numVertices * sizeof(glm::vec3);
    if (m_normals)
        estimatedSize += m_numVertices * sizeof(glm::vec3);
    if (m_tangents)
        estimatedSize += m_numVertices * sizeof(glm::vec3);
    if (m_uvCoords)
        estimatedSize += m_numVertices * sizeof(glm::vec2);

    flatbuffers::FlatBufferBuilder builder(estimatedSize);
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

    auto triangleMesh = serialization::CreateTriangleMesh(
        builder,
        m_numTriangles,
        m_numVertices,
        triangles,
        positions,
        normals,
        tangents,
        uvCoords);
    builder.Finish(triangleMesh);

    std::ofstream file;
    file.open(filename.data(), std::ios::out | std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
    file.close();
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

size_t TriangleMesh::size() const
{
    size_t sizeBytes = sizeof(decltype(*this));
    sizeBytes += sizeof(glm::ivec3) * m_numTriangles;
    sizeBytes += sizeof(glm::vec3) * m_numVertices;
    if (m_normals)
        sizeBytes += sizeof(glm::vec3) * m_numVertices;
    if (m_tangents)
        sizeBytes += sizeof(glm::vec3) * m_numVertices;
    if (m_uvCoords)
        sizeBytes += sizeof(glm::vec2) * m_numVertices;

    return sizeBytes;
}

}

/*
bool TriangleMesh::intersectMollerTrumbore(unsigned int primitiveIndex, Ray& ray) const
{
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    const float EPSILON = 0.000001f;

    Triangle triangle = m_triangles[primitiveIndex];
    glm::vec3 p0 = m_positions[triangle.i0];
    glm::vec3 p1 = m_positions[triangle.i1];
    glm::vec3 p2 = m_positions[triangle.i2];

    glm::vec3 e1 = p1 - p0;
    glm::vec3 e2 = p2 - p0;
    glm::vec3 h = cross(ray.direction, e2);
    float a = dot(e1, h);
    if (a > -EPSILON && a < EPSILON)
        return false;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - p0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return false;

    glm::vec3 q = cross(s, e1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f)
        return false;

    float t = f * dot(e2, q);
    if (t > EPSILON) {
        ray.t = t;
        ray.uv = glm::vec2(u, v);
        return true;
    }

    return false;
}

bool TriangleMesh::intersectPbrt(unsigned primitiveIndex, Ray& ray) const
{
    // Based on PBRT v3 triangle intersection test:
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    //
    // Transform the ray and triangle such that the ray origin is at (0,0,0) and its
    // direction points along the +Z axis. This makes the intersection test easy and
    // allows for watertight intersection testing.
    Triangle triangle = m_triangles[primitiveIndex];
    glm::vec3 p0 = m_positions[triangle.i0];
    glm::vec3 p1 = m_positions[triangle.i1];
    glm::vec3 p2 = m_positions[triangle.i2];

    // Translate vertices based on ray origin
    glm::vec3 p0t = p0 - ray.origin;
    glm::vec3 p1t = p1 - ray.origin;
    glm::vec3 p2t = p2 - ray.origin;

    // Permutate components of triangle vertices and ray direction
    int kz = maxDimension(abs(ray.direction));
    int kx = kz + 1;
    if (kx == 3)
        kx = 0;
    int ky = kx + 1;
    if (ky == 3)
        ky = 0;
    //int kx = (kz + 1) % 3;
    //int ky = (kz + 2) % 3;
    glm::vec3 d = permute(ray.direction, kx, ky, kz);
    p0t = permute(p0t, kx, ky, kz);
    p1t = permute(p1t, kx, ky, kz);
    p2t = permute(p2t, kx, ky, kz);

    // Apply shear transformation to translated vertex positions
    // Aligns the ray direction with the +z axis. Only shear x and y dimensions,
    // we can wait and shear the z dimension only if the ray actually intersects
    // the triangle.
    //TODO(Mathijs): consider precomputing and storing the shear values in the ray
    float Sx = -d.x / d.z;
    float Sy = -d.y / d.z;
    float Sz = 1.0f / d.z;
    p0t.x += Sx * p0t.z;
    p0t.y += Sy * p0t.z;
    p1t.x += Sx * p1t.z;
    p1t.y += Sy * p1t.z;
    p2t.x += Sx * p2t.z;
    p2t.y += Sy * p2t.z;

    // Compute the edge function coefficients
    float e0 = p1t.x * p2t.y - p1t.y * p2t.x;
    float e1 = p2t.x * p0t.y - p2t.y * p0t.x;
    float e2 = p0t.x * p1t.y - p0t.y * p1t.x;

    // Fall back to double precision test at triangle edges
    if (e0 == 0.0f || e1 == 0.0f || e2 == 0.0f) {
        double p2txp1ty = (double)p2t.x * (double)p1t.y;
        double p2typ1tx = (double)p2t.y * (double)p1t.x;
        e0 = (float)(p2typ1tx - p2txp1ty);
        double p0txp2ty = (double)p0t.x * (double)p2t.y;
        double p0typ2tx = (double)p0t.y * (double)p2t.x;
        e1 = (float)(p0typ2tx - p0txp2ty);
        double p1txp0ty = (double)p1t.x * (double)p0t.y;
        double p1typ0tx = (double)p1t.y * (double)p0t.x;
        e2 = (float)(p1typ0tx - p1txp0ty);
    }

    // If the signs of the edge function values differ, then the point (0, 0) is not
    // on the same side of all three edges and therefor is outside the triangle.
    if ((e0 < 0.0f || e1 < 0.0f || e2 < 0.0f) && ((e0 > 0.0f || e1 > 0.0f || e2 > 0.0f)))
        return false;

    // If the sum of the three ege function values is zero, then the ray is
    // approaching the triangle edge-on, and we report no intersection.
    float det = e0 + e1 + e2;
    if (det == 0.0f)
        return false;

    // Compute scaled hit distance to triangle and test against t range
    p0t.z *= Sz;
    p1t.z *= Sz;
    p2t.z *= Sz;
    float tScaled = e0 * p0t.z + e1 * p1t.z + e2 * p2t.z;
    if (det < 0.0f && (tScaled >= 0.0f || tScaled < ray.t * det))
        return false;
    else if (det > 0.0f && (tScaled <= 0.0f || tScaled > ray.t * det))
        return false;

    // Compute barycentric coordinates and t value for triangle intersection
    float invDet = 1.0f / det;
    float b0 = e0 * invDet;
    float b1 = e1 * invDet;
    //float b2 = e2 * invDet;
    float t = tScaled * invDet;

    ray.t = t;
    ray.uv = glm::vec2(b0, b1);

    return true;
}*/
