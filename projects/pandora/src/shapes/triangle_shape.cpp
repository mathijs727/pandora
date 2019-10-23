#include "pandora/core/stats.h"
#include "pandora/flatbuffers/data_conversion.h"
#include "pandora/graphics_core/transform.h"
#include "pandora/shapes/triangle.h"
#include "pandora/utility/error_handling.h"
#include "pandora/utility/math.h"

#include "tinyply.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cassert>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
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

static std::vector<glm::vec3> transformPoints(std::vector<glm::vec3>&& points, const glm::mat4& matrix)
{
    pandora::Transform transform { matrix };
    std::vector<glm::vec3> out;
    out.resize(points.size());
    std::transform(std::begin(points), std::end(points), std::begin(out),
        [&](const glm::vec3& p) {
            return transform.transformPoint(p);
        });
    return out;
}

static std::vector<glm::vec3> transformNormals(std::vector<glm::vec3>&& normals, const glm::mat4& matrix)
{
    pandora::Transform transform { matrix };
    std::vector<glm::vec3> out;
    out.resize(normals.size());
    std::transform(std::begin(normals), std::end(normals), std::begin(out),
        [&](const glm::vec3& n) {
            return transform.transformNormal(n);
        });
    return out;
}

static std::vector<glm::vec3> transformVectors(std::vector<glm::vec3>&& vectors, const glm::mat4& matrix)
{
    pandora::Transform transform { matrix };
    std::vector<glm::vec3> out;
    out.resize(vectors.size());
    std::transform(std::begin(vectors), std::end(vectors), std::begin(out),
        [&](const glm::vec3& v) {
            return transform.transformVector(v);
        });
    return out;
}

namespace pandora {

TriangleShape TriangleShape::loadSerialized(const serialization::TriangleMesh* pSerializedTriangleMesh, const glm::mat4& transformMatrix)
{
    Transform transform(transformMatrix);

    std::vector<glm::uvec3> indices;
    indices.resize(pSerializedTriangleMesh->indices()->size());
    std::transform(
        pSerializedTriangleMesh->indices()->begin(),
        pSerializedTriangleMesh->indices()->end(),
        std::begin(indices),
        [](const serialization::Vec3u* t) {
            return deserialize(*t);
        });
    indices.shrink_to_fit();

    std::vector<glm::vec3> positions;
    positions.resize(pSerializedTriangleMesh->positions()->size());
    std::transform(
        pSerializedTriangleMesh->positions()->begin(),
        pSerializedTriangleMesh->positions()->end(),
        std::begin(positions),
        [&](const serialization::Vec3* p) {
            return transform.transformPoint(deserialize(*p));
        });
    positions.shrink_to_fit();

    std::vector<glm::vec3> normals;
    if (pSerializedTriangleMesh->normals()) {
        normals.resize(pSerializedTriangleMesh->normals()->size());
        std::transform(
            pSerializedTriangleMesh->normals()->begin(),
            pSerializedTriangleMesh->normals()->end(),
            std::begin(normals),
            [&](const serialization::Vec3* n) {
                return transform.transformNormal(deserialize(*n));
            });
    }
    normals.shrink_to_fit();

    std::vector<glm::vec3> tangents;
    if (pSerializedTriangleMesh->tangents()) {
        tangents.resize(pSerializedTriangleMesh->tangents()->size());
        std::transform(
            pSerializedTriangleMesh->tangents()->begin(),
            pSerializedTriangleMesh->tangents()->end(),
            std::begin(tangents),
            [&](const serialization::Vec3* t) {
                return transform.transformNormal(deserialize(*t));
            });
    }
    tangents.shrink_to_fit();

    std::vector<glm::vec2> texCoords;
    if (pSerializedTriangleMesh->texCoords()) {
        texCoords.resize(pSerializedTriangleMesh->texCoords()->size());
        std::transform(
            pSerializedTriangleMesh->texCoords()->begin(),
            pSerializedTriangleMesh->texCoords()->end(),
            std::begin(texCoords),
            [](const serialization::Vec2* uv) {
                return deserialize(*uv);
            });
    }
    texCoords.shrink_to_fit();

    return TriangleShape(std::move(indices), std::move(positions), std::move(normals), std::move(texCoords), std::move(tangents));
}

TriangleShape::TriangleShape(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec2>&& texCoords,
    std::vector<glm::vec3>&& tangents)
    // Cannot use std::make_shared to access private constructor of friend class
    : m_intersectGeometry(
        std::unique_ptr<TriangleIntersectGeometry>(
            new TriangleIntersectGeometry(std::move(indices), std::move(positions))))
    , m_shadingGeometry(
          std::unique_ptr<TriangleShadingGeometry>(
              new TriangleShadingGeometry(m_intersectGeometry.get(), std::move(normals), std::move(texCoords), std::move(tangents))))
    , m_bounds(m_intersectGeometry->computeBounds())
    , m_numPrimitives(m_intersectGeometry->numPrimitives())
{
}

TriangleShape::TriangleShape(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec2>&& texCoords,
    std::vector<glm::vec3>&& tangents,
    const glm::mat4& transform)
    // Cannot use std::make_shared to access private constructor of friend class
    : m_intersectGeometry(
        std::unique_ptr<TriangleIntersectGeometry>(
            new TriangleIntersectGeometry(std::move(indices), transformPoints(std::move(positions), transform))))
    , m_shadingGeometry(
          std::unique_ptr<TriangleShadingGeometry>(
              new TriangleShadingGeometry(
                  m_intersectGeometry.get(),
                  transformNormals(std::move(normals), transform),
                  std::move(texCoords),
                  transformPoints(std::move(tangents), transform))))
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
    std::vector<glm::vec2> texCoords;

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

    // UV mapping
    if (mesh->HasTextureCoords(0)) {
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            texCoords.push_back(glm::vec2(assimpVec(mesh->mTextureCoords[0][i])));
        }
    }

    return TriangleShape(
        std::move(indices),
        std::move(positions),
        std::move(normals),
        std::move(texCoords),
        std::move(tangents));
}

std::optional<TriangleShape> TriangleShape::loadFromFileSingleShape(std::filesystem::path filePath, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        spdlog::warn("Could not find mesh file \"{}\"", filePath.string());
        return {};
    }

    if (filePath.extension() == ".ply") {
        // Assimp is really slow so use tiny ply to parse ply files
        // https://github.com/ddiakopoulos/tinyply
        if (objTransform == glm::identity<glm::mat4>())
            return loadFromPlyFile(filePath, {});
        else
            return loadFromPlyFile(filePath, objTransform);
    } else {
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
        {},
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

flatbuffers::Offset<serialization::TriangleMesh> TriangleShape::serialize(flatbuffers::FlatBufferBuilder& builder) const
{
    auto triangles = builder.CreateVectorOfStructs(
        reinterpret_cast<const serialization::Vec3u*>(m_intersectGeometry->m_indices.data()), m_intersectGeometry->m_indices.size());
    auto positions = builder.CreateVectorOfStructs(
        reinterpret_cast<const serialization::Vec3*>(m_intersectGeometry->m_positions.data()), m_intersectGeometry->m_positions.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec3*>> normals = 0;
    if (!m_shadingGeometry->m_normals.empty())
        normals = builder.CreateVectorOfStructs(
            reinterpret_cast<const serialization::Vec3*>(m_shadingGeometry->m_normals.data()), m_shadingGeometry->m_normals.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec3*>> tangents = 0;
    if (!m_shadingGeometry->m_tangents.empty())
        tangents = builder.CreateVectorOfStructs(
            reinterpret_cast<const serialization::Vec3*>(m_shadingGeometry->m_tangents.data()), m_shadingGeometry->m_tangents.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec2*>> texCoords = 0;
    if (!m_shadingGeometry->m_texCoords.empty())
        texCoords = builder.CreateVectorOfStructs(
            reinterpret_cast<const serialization::Vec2*>(m_shadingGeometry->m_texCoords.data()), m_shadingGeometry->m_texCoords.size());

    auto bounds = m_bounds.serialize();
    return serialization::CreateTriangleMesh(
        builder,
        triangles,
        positions,
        normals,
        tangents,
        texCoords,
        &bounds);
}

std::optional<TriangleShape> TriangleShape::loadFromPlyFile(std::filesystem::path filePath, std::optional<glm::mat4> optTransform)
{
    std::ifstream ss(filePath, std::ios::binary);
    if (ss.fail()) {
        spdlog::error("Failed to open plyfile \"{}\"", filePath.string());
        return {};
    }

    tinyply::PlyFile file;
    file.parse_header(ss);

    bool hasUvCoords = false;
    bool hasNormals = false;
    for (auto e : file.get_elements()) {
        if (e.name == "vertex") {
            for (auto p : e.properties) {
                if (p.name == "nx")
                    hasNormals = true;
                if (p.name == "u")
                    hasUvCoords = true;
            }
        }
    }

    std::shared_ptr<tinyply::PlyData> plyIndices = file.request_properties_from_element("face", { "vertex_indices" });
    std::shared_ptr<tinyply::PlyData> plyVertices = file.request_properties_from_element("vertex", { "x", "y", "z" });
    std::shared_ptr<tinyply::PlyData> plyNormals;
    if (hasNormals)
        plyNormals = file.request_properties_from_element("vertex", { "nx", "ny", "nz" });
    std::shared_ptr<tinyply::PlyData> plyUvCoords;
    if (hasUvCoords)
        plyUvCoords = file.request_properties_from_element("vertex", { "u", "v" });

    file.read(ss);

    std::vector<glm::uvec3> indices { plyIndices->count };
    size_t size = plyIndices->buffer.size_bytes();
    std::memcpy(indices.data(), plyIndices->buffer.get(), size);

    std::vector<glm::vec3> positions { plyVertices->count };
    std::memcpy(positions.data(), plyVertices->buffer.get(), plyVertices->buffer.size_bytes());

    std::vector<glm::vec3> normals;
    if (hasNormals) {
        normals.resize(plyNormals->count);
        std::memcpy(normals.data(), plyNormals->buffer.get(), plyNormals->buffer.size_bytes());
    }

    std::vector<glm::vec2> texCoords;
    if (hasUvCoords) {
        texCoords.resize(plyUvCoords->count);
        std::memcpy(texCoords.data(), plyUvCoords->buffer.get(), plyUvCoords->buffer.size_bytes());
    }

    // TODO
    std::vector<glm::vec3> tangents;

    if (optTransform) {
        positions = transformPoints(std::move(positions), *optTransform);
        normals = transformNormals(std::move(normals), *optTransform);
        tangents = transformPoints(std::move(tangents), *optTransform);
    }

    return TriangleShape(std::move(indices), std::move(positions), std::move(normals), std::move(texCoords), std::move(tangents));
}

}
