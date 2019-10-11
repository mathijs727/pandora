#include "pandora/core/stats.h"
#include "pandora/flatbuffers/data_conversion.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/shapes/triangle.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <spdlog/spdlog.h>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>

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

TriangleShape::TriangleShape(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec2>&& uvCoords)
    // Cannot use std::make_shared to access private constructor of friend class
    : m_intersectGeometry(
        std::shared_ptr<TriangleIntersectGeometry>(
            new TriangleIntersectGeometry(std::move(indices), std::move(positions))))
    , m_shadingGeometry(
          std::shared_ptr<TriangleShadingGeometry>(
              new TriangleShadingGeometry(m_intersectGeometry, std::move(normals), std::move(uvCoords))))
    , m_bounds(m_intersectGeometry->computeBounds())
    , m_numPrimitives(m_intersectGeometry->numPrimitives())
{
}

const IntersectGeometry* TriangleShape::getIntersectGeometry() const
{
    return m_intersectGeometry.get();
}

const ShadingGeometry* TriangleShape::getShadingGeometry() const
{
    return m_shadingGeometry.get();
}

unsigned TriangleShape::numPrimitives() const
{
    return m_numPrimitives;
}

Bounds TriangleShape::getBounds() const
{
    return m_bounds;
}

TriangleShape TriangleShape::createAssimpMesh(const aiScene* scene, const unsigned meshIndex, const glm::mat4& matrix, bool ignoreVertexNormals)
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

    return TriangleShape(
        std::move(indices),
        std::move(positions),
        std::move(normals),
        //std::move(tangents),
        std::move(uvCoords));
}

std::optional<TriangleShape> TriangleShape::loadFromFileSingleShape(std::filesystem::path filePath, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        spdlog::warn("Could not find mesh file \"{}\"", filePath.string());
        return {};
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_GenNormals | aiProcess_Triangulate);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        spdlog::warn("Failed to load mesh file \"{}\"", filePath.string());
        return {};
    }

    auto ret = loadFromFileSingleShape(scene, objTransform, ignoreVertexNormals);
    importer.FreeScene();
    return std::move(ret);
}

std::optional<TriangleShape> TriangleShape::loadFromFileSingleShape(const aiScene* scene, glm::mat4 objTransform, bool ignoreVertexNormals)
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

    return TriangleShape(
        std::move(indices),
        std::move(positions),
        std::move(normals),
        {});
}

std::vector<TriangleShape> TriangleShape::loadFromFile(std::filesystem::path filePath, glm::mat4 modelTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        spdlog::warn("Could not find mesh file \"{}\"", filePath.string());
        return {};
    }

    Assimp::Importer importer;
    //const aiScene* scene = importer.ReadFile(filePath.string().data(),
    //    aiProcess_ValidateDataStructure | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_RemoveRedundantMaterials | aiProcess_Triangulate | aiProcess_GenNormals);
    //importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);
    const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_GenNormals | aiProcess_Triangulate);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        spdlog::warn("Failed to load mesh file \"{}\"", filePath.string());
        return {};
    }

    std::vector<TriangleShape> result;

    std::stack<std::tuple<aiNode*, glm::mat4>> stack;
    stack.push({ scene->mRootNode, modelTransform * assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto [node, transform] = stack.top();
        stack.pop();

        transform *= assimpMatrix(node->mTransformation);

        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            // Process subMesh
            result.push_back(createAssimpMesh(scene, node->mMeshes[i], transform, ignoreVertexNormals));
        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], transform });
        }
    }

    importer.FreeScene();
    return result;
}

}

/*TriangleMesh::TriangleMesh(const serialization::TriangleMesh* serializedTriangleMesh)
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
}*/
