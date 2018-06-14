#include "pandora/geometry/triangle.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "glm/mat4x4.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>
#include <tuple>

namespace pandora {

TriangleMesh::TriangleMesh(
    std::vector<glm::ivec3>&& indices,
    std::vector<glm::vec3>&& positions,
    std::vector<glm::vec3>&& normals)
    : m_numPrimitives((unsigned)indices.size())
    , m_primitiveBounds(m_numPrimitives)
    , m_indices(std::move(indices))
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
{
    assert(m_indices.size() > 0);
    assert(m_positions.size() == m_normals.size() || m_normals.size() == 0);

    for (unsigned i = 0; i < m_numPrimitives; i++) {
        glm::ivec3 indices = m_indices[i];
        Bounds& bounds = m_primitiveBounds[i];
        bounds.reset();
        bounds.grow(m_positions[indices.x]);
        bounds.grow(m_positions[indices.y]);
        bounds.grow(m_positions[indices.z]);
    }
}

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

static std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>> createMeshAssimp(const aiScene* scene, const unsigned meshIndex, const glm::mat4& transform)
{
    aiMesh* mesh = scene->mMeshes[meshIndex];

    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
        return { nullptr, nullptr };

    std::vector<glm::ivec3> indices;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    indices.reserve(mesh->mNumFaces);
    positions.reserve(mesh->mNumVertices);
    positions.reserve(mesh->mNumVertices);

    // Add all vertex data
    for (unsigned vertexIdx = 0; vertexIdx < mesh->mNumVertices; vertexIdx++) {
        glm::vec3 position = transform * glm::vec4(assimpVec(mesh->mVertices[vertexIdx]), 1);
        positions.push_back(position);
    }

    // Add all the triangle indices
    for (unsigned faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
        const aiFace& face = mesh->mFaces[faceIdx];
        if (face.mNumIndices != 3) {
            std::cout << "Found a face which is not a triangle, discarding!" << std::endl;
            continue;
        }

        auto aiIndices = face.mIndices;
        glm::ivec3 triangle = {
            static_cast<int>(aiIndices[0]),
            static_cast<int>(aiIndices[1]),
            static_cast<int>(aiIndices[2])
        };
        indices.push_back(triangle);
    }

    return { std::make_shared<TriangleMesh>(std::move(indices), std::move(positions), std::move(normals)), nullptr };
}

std::vector<std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>>> TriangleMesh::loadFromFile(const std::string_view filename, glm::mat4 modelTransform)
{
    if (!fileExists(filename)) {
        std::cout << "Could not find mesh file: " << filename << std::endl;
        return {};
    }

    //TODO(Mathijs): move this out of the triangle class because it should also load material information
    // which will be stored in a SceneObject kinda way.
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.data(), aiProcessPreset_TargetRealtime_MaxQuality);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        std::cout << "Failed to load mesh file: " << filename << std::endl;
        return {};
    }

    std::vector<std::pair<std::shared_ptr<TriangleMesh>, std::shared_ptr<Material>>> result;

    std::stack<std::tuple<aiNode*, glm::mat4>> stack;
    stack.push({ scene->mRootNode, modelTransform * assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto [node, transform] = stack.top();
        stack.pop();

        transform *= assimpMatrix(node->mTransformation);

        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            // Process subMesh
            result.push_back(createMeshAssimp(scene, node->mMeshes[i], transform));
        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], transform });
        }
    }

    return result;
}

unsigned TriangleMesh::numPrimitives() const
{
    return m_numPrimitives;
}

const gsl::span<const glm::ivec3> TriangleMesh::getIndices() const
{
    return m_indices;
}

const gsl::span<const glm::vec3> TriangleMesh::getPositions() const
{
    return m_positions;
}

const gsl::span<const glm::vec3> TriangleMesh::getNormals() const
{
    return m_normals;
}
}

/*
bool TriangleMesh::intersectMollerTrumbore(unsigned int primitiveIndex, Ray& ray) const
{
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    const float EPSILON = 0.000001f;

    Triangle triangle = m_indices[primitiveIndex];
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
    Triangle triangle = m_indices[primitiveIndex];
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
