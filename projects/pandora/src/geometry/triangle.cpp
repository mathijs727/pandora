#include "pandora/geometry/triangle.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/mat4x4.hpp"
#include "pandora/core/stats.h"
#include "pandora/flatbuffers/data_conversion.h"
#include "pandora/graphics_core/transform.h"
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
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec3>&& tangents,
    std::vector<glm::vec2>&& uvCoords)
    : m_indices(std::move(indices))
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
    , m_tangents(std::move(tangents))
    , m_uvCoords(std::move(uvCoords))
{
    ALWAYS_ASSERT(normals.empty() || normals.size() == positions.size());

    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
    m_tangents.shrink_to_fit();
    m_uvCoords.shrink_to_fit();

    for (const glm::vec3& p : m_positions) {
        m_bounds.grow(p);
    }

    // Ignore memory required by the class itself
    g_stats.memory.geometryLoaded += sizeBytes() - sizeof(decltype(*this));
}

TriangleMesh::TriangleMesh(const serialization::TriangleMesh* serializedTriangleMesh)
{
    m_indices.resize(serializedTriangleMesh->triangles()->size());
    std::transform(
        serializedTriangleMesh->triangles()->begin(),
        serializedTriangleMesh->triangles()->end(),
        std::begin(m_indices),
        [](const serialization::Vec3i* t) {
            return deserialize(*t);
        });
    m_indices.shrink_to_fit();

    m_positions.resize(serializedTriangleMesh->positions()->size());
    std::transform(
        serializedTriangleMesh->positions()->begin(),
        serializedTriangleMesh->positions()->end(),
        std::begin(m_positions),
        [](const serialization::Vec3* p) {
            return deserialize(*p);
        });
    m_positions.shrink_to_fit();

    m_normals.clear();
    if (serializedTriangleMesh->normals()) {
        m_normals.resize(serializedTriangleMesh->normals()->size());
        std::transform(
            serializedTriangleMesh->normals()->begin(),
            serializedTriangleMesh->normals()->end(),
            std::begin(m_normals),
            [](const serialization::Vec3* n) {
                return deserialize(*n);
            });
    }
    m_normals.shrink_to_fit();

    m_tangents.clear();
    if (serializedTriangleMesh->tangents()) {
        m_tangents.resize(serializedTriangleMesh->tangents()->size());
        std::transform(
            serializedTriangleMesh->tangents()->begin(),
            serializedTriangleMesh->tangents()->end(),
            std::begin(m_tangents),
            [](const serialization::Vec3* t) {
                return deserialize(*t);
            });
    }
    m_tangents.shrink_to_fit();

    m_uvCoords.clear();
    if (serializedTriangleMesh->uvCoords()) {
        m_uvCoords.resize(serializedTriangleMesh->uvCoords()->size());
        std::transform(
            serializedTriangleMesh->uvCoords()->begin(),
            serializedTriangleMesh->uvCoords()->end(),
            std::begin(m_uvCoords),
            [](const serialization::Vec2* uv) {
                return deserialize(*uv);
            });
    }
    m_uvCoords.shrink_to_fit();

    m_bounds = Bounds(*serializedTriangleMesh->bounds());

    g_stats.memory.geometryLoaded += sizeBytes() - sizeof(decltype(*this));
}

TriangleMesh::TriangleMesh(const serialization::TriangleMesh* serializedTriangleMesh, const glm::mat4& transformMatrix)
{
    Transform transform(transformMatrix);

    m_indices.resize(serializedTriangleMesh->triangles()->size());
    std::transform(
        serializedTriangleMesh->triangles()->begin(),
        serializedTriangleMesh->triangles()->end(),
        std::begin(m_indices),
        [&](const serialization::Vec3i* t) {
            return deserialize(*t);
        });
    m_indices.shrink_to_fit();

    m_positions.resize(serializedTriangleMesh->positions()->size());
    std::transform(
        serializedTriangleMesh->positions()->begin(),
        serializedTriangleMesh->positions()->end(),
        std::begin(m_positions),
        [&](const serialization::Vec3* p) {
            return transform.transformPoint(deserialize(*p));
        });
    m_positions.shrink_to_fit();

    m_normals.clear();
    if (serializedTriangleMesh->normals()) {
        m_normals.resize(serializedTriangleMesh->normals()->size());
        std::transform(
            serializedTriangleMesh->normals()->begin(),
            serializedTriangleMesh->normals()->end(),
            std::begin(m_normals),
            [&](const serialization::Vec3* n) {
                return transform.transformNormal(deserialize(*n));
            });
    }
    m_normals.shrink_to_fit();

    m_tangents.clear();
    if (serializedTriangleMesh->tangents()) {
        m_tangents.resize(serializedTriangleMesh->tangents()->size());
        std::transform(
            serializedTriangleMesh->tangents()->begin(),
            serializedTriangleMesh->tangents()->end(),
            std::begin(m_tangents),
            [&](const serialization::Vec3* t) {
                return transform.transformNormal(deserialize(*t));
            });
    }
    m_tangents.shrink_to_fit();

    m_uvCoords.clear();
    if (serializedTriangleMesh->uvCoords()) {
        m_uvCoords.resize(serializedTriangleMesh->uvCoords()->size());
        std::transform(
            serializedTriangleMesh->uvCoords()->begin(),
            serializedTriangleMesh->uvCoords()->end(),
            std::begin(m_uvCoords),
            [&](const serialization::Vec2* uv) {
                return deserialize(*uv);
            });
    }
    m_uvCoords.shrink_to_fit();

    m_bounds = Bounds(*serializedTriangleMesh->bounds());

    g_stats.memory.geometryLoaded += sizeBytes() - sizeof(decltype(*this));
}

TriangleMesh::~TriangleMesh()
{
    g_stats.memory.geometryEvicted += sizeBytes() - sizeof(decltype(*this));
}

flatbuffers::Offset<serialization::TriangleMesh> TriangleMesh::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    /*size_t estimatedSize = sizeof(this);
    estimatedSize += m_triangles.size() * sizeof(glm::ivec3)j;
    estimatedSize += m_positions.size() * sizeof(glm::vec3);
    estimatedSize += m_normals.size() * sizeof(glm::vec3);
    estimatedSize += m_tangents.size() * sizeof(glm::vec3);
    estimatedSize += m_uvCoords.size() * sizeof(glm::vec2);*/

    auto triangles = builder.CreateVectorOfStructs(reinterpret_cast<const serialization::Vec3i*>(m_indices.data()), m_indices.size());
    auto positions = builder.CreateVectorOfStructs(reinterpret_cast<const serialization::Vec3*>(m_positions.data()), m_positions.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec3*>> normals = 0;
    if (!m_normals.empty())
        normals = builder.CreateVectorOfStructs(reinterpret_cast<const serialization::Vec3*>(m_normals.data()), m_normals.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec3*>> tangents = 0;
    if (!m_tangents.empty())
        tangents = builder.CreateVectorOfStructs(reinterpret_cast<const serialization::Vec3*>(m_tangents.data()), m_tangents.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec2*>> uvCoords = 0;
    if (!m_uvCoords.empty())
        uvCoords = builder.CreateVectorOfStructs(reinterpret_cast<const serialization::Vec2*>(m_uvCoords.data()), m_uvCoords.size());

    auto bounds = m_bounds.serialize();
    return serialization::CreateTriangleMesh(
        builder,
        triangles,
        positions,
        normals,
        tangents,
        uvCoords,
        &bounds);
}

TriangleMesh TriangleMesh::subMesh(gsl::span<const unsigned> primitives) const
{
    std::vector<bool> usedVertices(numVertices());
    std::fill(std::begin(usedVertices), std::end(usedVertices), false);
    for (const unsigned primitiveID : primitives) {
        glm::uvec3 triangle = m_indices[primitiveID];
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

    std::vector<glm::uvec3> indices;
    for (unsigned triangleIndex : primitives) {
        glm::uvec3 originalTriangle = m_indices[triangleIndex];
        glm::ivec3 triangle = {
            vertexIndexMapping[originalTriangle[0]],
            vertexIndexMapping[originalTriangle[1]],
            vertexIndexMapping[originalTriangle[2]]
        };
        indices.push_back(triangle);
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec2> uvCoords;
    for (unsigned vertexIndex = 0; vertexIndex < numVertices(); vertexIndex++) {
        if (usedVertices[vertexIndex]) {
            positions.push_back(m_positions[vertexIndex]);
            if (!m_normals.empty())
                normals.push_back(m_normals[vertexIndex]);
            if (!m_tangents.empty())
                tangents.push_back(m_tangents[vertexIndex]);
            if (!m_uvCoords.empty())
                uvCoords.push_back(m_uvCoords[vertexIndex]);
        }
    }

    return TriangleMesh(std::move(indices), std::move(positions), std::move(normals), std::move(tangents), std::move(uvCoords));
}

void TriangleMesh::subdivide()
{
    ALWAYS_ASSERT(m_normals.empty() || m_normals.size() == m_positions.size());
    ALWAYS_ASSERT(m_tangents.empty());
    ALWAYS_ASSERT(m_uvCoords.empty());

    std::vector<glm::uvec3> outIndices;
    outIndices.reserve(m_indices.size() * 3);
    for (const auto& triangle : m_indices) {
        glm::vec3 v0 = m_positions[triangle[0]];
        glm::vec3 v1 = m_positions[triangle[1]];
        glm::vec3 v2 = m_positions[triangle[2]];
        glm::vec3 v3 = (v0 + v1 + v2) / 3.0f;

        int newVertexID = static_cast<int>(m_positions.size());
        m_positions.push_back(v3);
        if (!m_normals.empty()) {
            m_normals.push_back(
                glm::normalize(
                    m_normals[triangle[0]] + m_normals[triangle[1]] + m_normals[triangle[2]]));
        }
        outIndices.push_back({ triangle[0], triangle[1], newVertexID });
        outIndices.push_back({ triangle[1], triangle[2], newVertexID });
        outIndices.push_back({ triangle[2], triangle[0], newVertexID });
    }
    m_indices = std::move(outIndices);
    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
}

TriangleMesh TriangleMesh::createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& matrix, bool ignoreVertexNormals)
{
    const aiMesh* mesh = scene->mMeshes[meshIndex];

    Transform transform(matrix);

    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
        THROW_ERROR("Empty mesh");

    std::vector<glm::uvec3> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec2> uvCoords;

    // Triangles
    for (unsigned i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            THROW_ERROR("Found a face which is not a triangle, discarding!");
        }

        auto aiIndices = face.mIndices;
        indices.push_back({ aiIndices[0], aiIndices[1], aiIndices[2] });
    }

    // Positions
    for (unsigned i = 0; i < mesh->mNumVertices; i++) {
        positions.push_back(transform.transformPoint(assimpVec(mesh->mVertices[i])));
    }

    // Normals
    if (mesh->HasNormals() && !ignoreVertexNormals) {
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            normals.push_back(transform.transformNormal(assimpVec(mesh->mNormals[i])));
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
        std::move(indices),
        std::move(positions),
        std::move(normals),
        std::move(tangents),
        std::move(uvCoords));
}

/*std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(gsl::span<std::byte> buffer, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(buffer.data(), buffer.size_bytes(), aiProcess_GenNormals, "obj");

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh from buffer");
        return {};
    }

    auto ret = loadFromFileSingleMesh(scene, objTransform, ignoreVertexNormals);
    importer.FreeScene();
    return std::move(ret);
}

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(std::filesystem::path filePath, size_t start, size_t length, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        LOG_WARNING("Could not find mesh file: "s + filePath.string());
        return {};
    }

    auto mmapFile = mio::mmap_source(filePath.string(), start, length);

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(mmapFile.data(), length, aiProcess_GenNormals, "obj");

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + filePath.string());
        return {};
    }

    auto ret = loadFromFileSingleMesh(scene, objTransform, ignoreVertexNormals);
    importer.FreeScene();
    return std::move(ret);
}*/

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(std::filesystem::path filePath, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        LOG_WARNING("Could not find mesh file: "s + filePath.string());
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_GenNormals | aiProcess_Triangulate);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + filePath.string());
        return {};
    }

    auto ret = loadFromFileSingleMesh(scene, objTransform, ignoreVertexNormals);
    importer.FreeScene();
    return std::move(ret);
}

std::optional<TriangleMesh> TriangleMesh::loadFromFileSingleMesh(const aiScene* scene, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    std::vector<glm::uvec3> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    indices.reserve(scene->mMeshes[0]->mNumFaces);
    positions.reserve(scene->mMeshes[0]->mNumVertices * 3);
    normals.reserve(scene->mMeshes[0]->mNumVertices * 3);

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
                indices.push_back(glm::uvec3 {
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

    return TriangleMesh(
        std::move(indices),
        std::move(positions),
        std::move(normals),
        {},
        {});
}

std::vector<TriangleMesh> TriangleMesh::loadFromFile(std::filesystem::path filePath, glm::mat4 modelTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        LOG_WARNING("Could not find mesh file: "s + filePath.string());
        return {};
    }

    Assimp::Importer importer;
    //const aiScene* scene = importer.ReadFile(filePath.string().data(),
    //    aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);
    //importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);
    const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_GenNormals | aiProcess_Triangulate);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        LOG_WARNING("Failed to load mesh file: "s + filePath.string());
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

    importer.FreeScene();
    return result;
}

size_t TriangleMesh::sizeBytes() const
{
    size_t size = sizeof(TriangleMesh);
    size += m_indices.size() * sizeof(glm::uvec3); // triangles
    size += m_positions.size() * sizeof(glm::vec3); // positions
    size += m_normals.size() * sizeof(glm::vec3); // normals
    size += m_tangents.size() * sizeof(glm::vec3); // tangents
    size += m_uvCoords.size() * sizeof(glm::vec2); // uv coords
    return size;
}

unsigned TriangleMesh::numTriangles() const
{
    return static_cast<unsigned>(m_indices.size());
}

unsigned TriangleMesh::numVertices() const
{
    return static_cast<unsigned>(m_positions.size());
}

gsl::span<const glm::uvec3> TriangleMesh::getIndices() const
{
    return m_indices;
}

gsl::span<const glm::vec3> TriangleMesh::getPositions() const
{
    return m_positions;
}

gsl::span<const glm::vec3> TriangleMesh::getNormals() const
{
    return m_normals;
}

gsl::span<const glm::vec3> TriangleMesh::getTangents() const
{
    return m_tangents;
}

gsl::span<const glm::vec2> TriangleMesh::getUVCoords() const
{
    return m_uvCoords;
}

Bounds TriangleMesh::getBounds() const
{
    return m_bounds;
}

Bounds TriangleMesh::getPrimitiveBounds(unsigned primitiveID) const
{
    glm::uvec3 triangle = m_indices[primitiveID];

    Bounds bounds;
    bounds.grow(m_positions[triangle[0]]);
    bounds.grow(m_positions[triangle[1]]);
    bounds.grow(m_positions[triangle[2]]);
    return bounds;
}

RTCGeometry TriangleMesh::createEmbreeGeometry(RTCDevice embreeDevice) const
{

    RTCGeometry embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
    rtcSetSharedGeometryBuffer(
        embreeGeometry,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_UINT,
        m_indices.data(),
        0,
        sizeof(uint32_t),
        m_indices.size() * 3);
    static_assert(sizeof(glm::uvec3) == 3 * sizeof(uint32_t));

    rtcSetSharedGeometryBuffer(
        embreeGeometry,
        RTC_BUFFER_TYPE_INDEX,
        0,
        RTC_FORMAT_FLOAT3,
        m_positions.data(),
        0,
        sizeof(glm::vec3),
        m_positions.size());
    return embreeGeometry;
}

float TriangleMesh::primitiveArea(unsigned primitiveID) const
{
    glm::uvec3 triangle = m_indices[primitiveID];
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

    glm::uvec3 triangle = m_indices[primitiveID];
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
    if (!m_uvCoords.empty()) {
        glm::uvec3 triangle = m_indices[primitiveID];
        uv[0] = m_uvCoords[triangle[0]];
        uv[1] = m_uvCoords[triangle[1]];
        uv[2] = m_uvCoords[triangle[2]];
    } else {
        uv[0] = glm::vec2(0, 0);
        uv[1] = glm::vec2(1, 0);
        uv[2] = glm::vec2(1, 1);
    }
}

void TriangleMesh::getPs(unsigned primitiveID, gsl::span<glm::vec3, 3> p) const
{
    glm::ivec3 triangle = m_indices[primitiveID];
    p[0] = m_positions[triangle[0]];
    p[1] = m_positions[triangle[1]];
    p[2] = m_positions[triangle[2]];
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

    for (glm::uvec3 triangle : m_indices) {
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
