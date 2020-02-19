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
#include <optick.h>
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
            return transform.transformPointToWorld(p);
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
            return transform.transformNormalToWorld(n);
        });
    return out;
}

namespace pandora {

TriangleShape::TriangleShape(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec2>&& texCoords)
    : Shape(true)
    , m_numPrimitives(static_cast<unsigned>(indices.size()))
    , m_indices(std::move(indices))
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
    , m_texCoords(std::move(texCoords))
    , m_serializedStateHandle()
{
    Bounds bounds;
    for (const glm::vec3& p : m_positions)
        bounds.grow(p);
    m_bounds = bounds;

    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
    m_texCoords.shrink_to_fit();

    ALWAYS_ASSERT(m_indices.size() < std::numeric_limits<unsigned>::max());
    g_stats.memory.geometryLoaded += sizeBytes();
}

TriangleShape::TriangleShape(
    std::vector<glm::uvec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals,
    std::vector<glm::vec2>&& texCoords,
    const glm::mat4& transform)
    : Shape(true)
    , m_numPrimitives(static_cast<unsigned>(indices.size()))
    , m_indices(std::move(indices))
    , m_positions(transformPoints(std::move(positions), transform))
    , m_normals(transformNormals(std::move(normals), transform))
    , m_texCoords(std::move(texCoords))
{
    Bounds bounds;
    for (const glm::vec3& p : m_positions)
        bounds.grow(p);
    m_bounds = bounds;

    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
    m_texCoords.shrink_to_fit();

    ALWAYS_ASSERT(indices.size() < std::numeric_limits<unsigned>::max());
    g_stats.memory.geometryLoaded += sizeBytes();
}

void TriangleShape::subdivide()
{
    ALWAYS_ASSERT(m_normals.empty() || m_normals.size() == m_positions.size());
    ALWAYS_ASSERT(m_texCoords.empty());

    size_t sizeBefore = sizeBytes();

    std::vector<glm::uvec3> outIndices;
    outIndices.reserve(m_indices.size() * 3);
    for (const auto& triangle : m_indices) {
        glm::vec3 v0 = m_positions[triangle[0]];
        glm::vec3 v1 = m_positions[triangle[1]];
        glm::vec3 v2 = m_positions[triangle[2]];
        glm::vec3 v3 = (v0 + v1 + v2) / 3.0f;

        unsigned newVertexID = static_cast<unsigned>(m_positions.size());
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

    ALWAYS_ASSERT(m_indices.size() < std::numeric_limits<unsigned>::max());
    m_numPrimitives = static_cast<unsigned>(m_indices.size());

    g_stats.memory.geometryLoaded += (sizeBytes() - sizeBefore);
}

void TriangleShape::doEvict()
{
    OPTICK_EVENT();

    g_stats.memory.geometryEvicted += sizeBytes();

    m_indices.clear();
    m_positions.clear();
    m_normals.clear();
    m_texCoords.clear();

    m_indices.shrink_to_fit();
    m_positions.shrink_to_fit();
    m_normals.shrink_to_fit();
    m_texCoords.shrink_to_fit();
}

void TriangleShape::doMakeResident(tasking::Deserializer& deserializer)
{
    OPTICK_EVENT();

    const void* pData = deserializer.map(m_serializedStateHandle);
    const auto* pSerializedTriangleMesh = serialization::GetTriangleMesh(pData);

    m_indices.resize(pSerializedTriangleMesh->indices()->size());
    std::transform(
        pSerializedTriangleMesh->indices()->begin(),
        pSerializedTriangleMesh->indices()->end(),
        std::begin(m_indices),
        [](const serialization::Vec3u* t) {
            return deserialize(*t);
        });
    m_indices.shrink_to_fit();

    m_positions.resize(pSerializedTriangleMesh->positions()->size());
    std::transform(
        pSerializedTriangleMesh->positions()->begin(),
        pSerializedTriangleMesh->positions()->end(),
        std::begin(m_positions),
        [](const serialization::Vec3* p) {
            return deserialize(*p);
        });
    m_positions.shrink_to_fit();

    if (pSerializedTriangleMesh->normals()) {
        m_normals.resize(pSerializedTriangleMesh->normals()->size());
        std::transform(
            pSerializedTriangleMesh->normals()->begin(),
            pSerializedTriangleMesh->normals()->end(),
            std::begin(m_normals),
            [](const serialization::Vec3* n) {
                return deserialize(*n);
            });
        m_normals.shrink_to_fit();
    }

    if (pSerializedTriangleMesh->texCoords()) {
        m_texCoords.resize(pSerializedTriangleMesh->texCoords()->size());
        std::transform(
            pSerializedTriangleMesh->texCoords()->begin(),
            pSerializedTriangleMesh->texCoords()->end(),
            std::begin(m_texCoords),
            [](const serialization::Vec2* uv) {
                return deserialize(*uv);
            });
        m_texCoords.shrink_to_fit();
    }

    deserializer.unmap(m_serializedStateHandle);

    g_stats.memory.geometryLoaded += sizeBytes();
}

unsigned TriangleShape::numPrimitives() const
{
    return m_numPrimitives;
}

Bounds TriangleShape::getBounds() const
{
    return m_bounds;
}

Bounds TriangleShape::getPrimitiveBounds(unsigned primitiveID) const
{
    const glm::uvec3 triangle = m_indices[primitiveID];

    Bounds bounds;
    bounds.grow(m_positions[triangle.x]);
    bounds.grow(m_positions[triangle.y]);
    bounds.grow(m_positions[triangle.z]);
    return bounds;
}

TriangleShape TriangleShape::subMesh(gsl::span<const unsigned> primitives) const
{
    std::vector<bool> usedVertices;
    std::fill_n(std::back_inserter(usedVertices), m_positions.size(), false);
    for (const unsigned primitiveID : primitives) {
        const auto& triangle = m_indices[primitiveID];
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
        glm::uvec3 triangle = {
            vertexIndexMapping[originalTriangle[0]],
            vertexIndexMapping[originalTriangle[1]],
            vertexIndexMapping[originalTriangle[2]]
        };
        indices.push_back(triangle);
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    for (size_t vertexIndex = 0; vertexIndex < m_positions.size(); vertexIndex++) {
        if (usedVertices[vertexIndex]) {
            positions.push_back(m_positions[vertexIndex]);
            if (!m_normals.empty())
                normals.push_back(m_normals[vertexIndex]);
            if (!m_texCoords.empty())
                texCoords.push_back(m_texCoords[vertexIndex]);
        }
    }

    return TriangleShape(std::move(indices), std::move(positions), std::move(normals), std::move(texCoords));
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
        positions.push_back(transform.transformPointToWorld(assimpVec(mesh->mVertices[i])));
    }

    // Normals
    if (mesh->HasNormals() && !ignoreVertexNormals) {
        for (unsigned i = 0; i < mesh->mNumVertices; i++) {
            normals.push_back(transform.transformNormalToWorld(assimpVec(mesh->mNormals[i])));
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
        std::move(texCoords));
}

std::optional<TriangleShape> TriangleShape::loadFromFileSingleShape(std::filesystem::path filePath, glm::mat4 objTransform, bool ignoreVertexNormals)
{
    if (!std::filesystem::exists(filePath)) {
        spdlog::warn("Could not find mesh file \"{}\"", filePath.string());
        return {};
    }

    /*if (filePath.extension() == ".ply") {
        // Assimp is really slow so use tiny ply to parse ply files
        // https://github.com/ddiakopoulos/tinyply
        if (objTransform == glm::identity<glm::mat4>())
            return loadFromPlyFile(filePath, {});
        else
            return loadFromPlyFile(filePath, objTransform);
    } else*/
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_GenNormals | aiProcess_Triangulate);

        if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
            spdlog::warn("Failed to load mesh file \"{}\"", filePath.string());
            return {};
        }

        auto ret = loadFromFileSingleShape(scene, objTransform, ignoreVertexNormals);
        importer.FreeScene();
        return ret;
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
                glm::vec3 transformedPos = transform.transformPointToWorld(pos);
                positions.push_back(transformedPos);
            }

            // Normals
            if (mesh->HasNormals() && !ignoreVertexNormals) {
                for (unsigned j = 0; j < mesh->mNumVertices; j++) {
                    normals.push_back(transform.transformNormalToWorld(assimpVec(mesh->mNormals[j])));
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
    const aiScene* scene = importer.ReadFile(filePath.string().data(), aiProcess_Triangulate);

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

void TriangleShape::serialize(tasking::Serializer& serializer)
{
    flatbuffers::FlatBufferBuilder builder;
    auto triangles = builder.CreateVectorOfStructs(
        reinterpret_cast<const serialization::Vec3u*>(m_indices.data()), m_indices.size());
    auto positions = builder.CreateVectorOfStructs(
        reinterpret_cast<const serialization::Vec3*>(m_positions.data()), m_positions.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec3*>> normals = 0;
    if (!m_normals.empty())
        normals = builder.CreateVectorOfStructs(
            reinterpret_cast<const serialization::Vec3*>(m_normals.data()), m_normals.size());
    flatbuffers::Offset<flatbuffers::Vector<const serialization::Vec2*>> texCoords = 0;
    if (!m_texCoords.empty())
        texCoords = builder.CreateVectorOfStructs(
            reinterpret_cast<const serialization::Vec2*>(m_texCoords.data()), m_texCoords.size());

    auto bounds = m_bounds.serialize();
    auto triangleMesh = serialization::CreateTriangleMesh(
        builder,
        triangles,
        positions,
        normals,
        texCoords,
        &bounds);
    builder.Finish(triangleMesh);

    const void* pSerializedBuffer = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    auto [allocation, pOutBuffer] = serializer.allocateAndMap(size);
    std::memcpy(pOutBuffer, pSerializedBuffer, size);
    serializer.unmapPreviousAllocations();

    m_serializedStateHandle = allocation;
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

    bool hasIndices = false;
    bool hasTexCoords = false;
    bool hasNormals = false;
    for (auto e : file.get_elements()) {
        if (e.name == "face") {
            for (auto p : e.properties) {
                if (p.name == "vertex_indices")
                    hasIndices = true;
            }
        } else if (e.name == "vertex") {
            for (auto p : e.properties) {
                if (p.name == "nx")
                    hasNormals = true;
                if (p.name == "u")
                    hasTexCoords = true;
            }
        }
    }

    std::shared_ptr<tinyply::PlyData> plyIndices;
    if (hasIndices)
        plyIndices = file.request_properties_from_element("face", { "vertex_indices" });
    std::shared_ptr<tinyply::PlyData> plyVertices = file.request_properties_from_element("vertex", { "x", "y", "z" });
    std::shared_ptr<tinyply::PlyData> plyNormals;
    if (hasNormals)
        plyNormals = file.request_properties_from_element("vertex", { "nx", "ny", "nz" });
    std::shared_ptr<tinyply::PlyData> plyTexCoords;
    if (hasTexCoords)
        plyTexCoords = file.request_properties_from_element("vertex", { "u", "v" });

    file.read(ss);

    std::vector<glm::uvec3> indices;
    if (hasIndices) {
        ALWAYS_ASSERT(plyIndices->count % 3 == 0);
        indices.resize(plyIndices->count);
        size_t size = plyIndices->buffer.size_bytes();
        std::memcpy(indices.data(), plyIndices->buffer.get(), size);
    } else {
        ALWAYS_ASSERT(plyVertices->count % 3 == 0);
        indices.reserve(plyVertices->count / 3);
        for (size_t i = 0; i < plyVertices->count; i += 3) {
            indices.push_back(glm::uvec3(i, i + 1, i + 2));
        }
    }

    std::vector<glm::vec3> positions { plyVertices->count };
    std::memcpy(positions.data(), plyVertices->buffer.get(), plyVertices->buffer.size_bytes());

    std::vector<glm::vec3> normals;
    if (hasNormals) {
        normals.resize(plyNormals->count);
        std::memcpy(normals.data(), plyNormals->buffer.get(), plyNormals->buffer.size_bytes());
    }

    std::vector<glm::vec2> texCoords;
    if (hasTexCoords) {
        texCoords.resize(plyTexCoords->count);
        std::memcpy(texCoords.data(), plyTexCoords->buffer.get(), plyTexCoords->buffer.size_bytes());
    }

    return TriangleShape(std::move(indices), std::move(positions), std::move(normals), std::move(texCoords));
}

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
            return transform.transformPointToWorld(deserialize(*p));
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
                return transform.transformNormalToWorld(deserialize(*n));
            });
    }
    normals.shrink_to_fit();

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

    return TriangleShape(std::move(indices), std::move(positions), std::move(normals), std::move(texCoords));
}
}
