#include "pandora/geometry/triangle.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "pandora/math/mat3x4.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>
#include <tuple>

namespace pandora {

TriangleMesh::TriangleMesh(
    std::vector<Triangle>&& indices,
    std::vector<Vec3f>&& positions,
    std::vector<Vec3f>&& normals)
    : m_numPrimitives(indices.size())
    , m_primitiveBounds(m_numPrimitives)
    , m_indices(std::move(indices))
    , m_positions(std::move(positions))
    , m_normals(std::move(normals))
{
    assert(indices.size() > 0);
    assert(positions.size() == normals.size() || normals.size() == 0);

    for (unsigned i = 0; i < m_numPrimitives; i++) {
        Triangle indices = m_indices[i];
        Bounds3f& bounds = m_primitiveBounds[i];
        bounds.reset();
        bounds.grow(m_positions[indices.i0]);
        bounds.grow(m_positions[indices.i1]);
        bounds.grow(m_positions[indices.i2]);
    }

    std::cout << "Triangles:\n";
    for (auto triangle : m_indices) {
        std::cout << triangle.i0 << ", " << triangle.i1 << ", " << triangle.i2 << "\n";
    }

    std::cout << "\nVertices:\n";
    for (auto vertex : m_positions) {
        std::cout << vertex << "\n";
    }

    std::cout.flush();
}

static bool fileExists(const std::string_view name)
{
    std::ifstream f(name.data());
    return f.good() && f.is_open();
}

static Mat3x4f assimpMatrix(const aiMatrix4x4& m)
{
    float values[3][4] = {};
    values[0][0] = m.a1;
    values[0][1] = m.b1;
    values[0][2] = m.c1;
    values[0][3] = m.d1;
    values[1][0] = m.a2;
    values[1][1] = m.b2;
    values[1][2] = m.c2;
    values[1][3] = m.d2;
    values[2][0] = m.a3;
    values[2][1] = m.b3;
    values[2][2] = m.c3;
    values[2][3] = m.d3;
    //std::cout << m.a1 << ", " << m.a2 << ", " << m.a3 << ", " << m.a4 << std::endl;
    //std::cout << m.b1 << ", " << m.b2 << ", " << m.b3 << ", " << m.b4 << std::endl;
    //std::cout << m.c1 << ", " << m.c2 << ", " << m.c3 << ", " << m.c4 << std::endl;
    //std::cout << m.d1 << ", " << m.d2 << ", " << m.d3 << ", " << m.d4 << std::endl;
    return Mat3x4f(values);
}

static Vec3f assimpVec(const aiVector3D& v)
{
    return Vec3f(v.x, v.y, v.z);
}

static void addSubMesh(const aiScene* scene,
    const unsigned meshIndex,
    const Mat3x4f& transformMatrix,
    std::vector<TriangleMesh::Triangle>& indices,
    std::vector<Vec3f>& positions,
    std::vector<Vec3f>& normals)
{
    aiMesh* mesh = scene->mMeshes[meshIndex];

    if (mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
        return;

    // Add all vertex data
    unsigned vertexOffset = positions.size();
    for (unsigned vertexIdx = 0; vertexIdx < mesh->mNumVertices; vertexIdx++) {
        //Vec3f position = transformMatrix.transformPoint(assimpVec(mesh->mVertices[vertexIdx]));
        Vec3f position = assimpVec(mesh->mVertices[vertexIdx]);
        //position = transformMatrix.transformPoint(position);
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
        TriangleMesh::Triangle triangle = {
            aiIndices[0] + vertexOffset,
            aiIndices[1] + vertexOffset,
            aiIndices[2] + vertexOffset
        };
        indices.push_back(triangle);
    }
}

std::unique_ptr<TriangleMesh> TriangleMesh::singleTriangle()
{
    std::vector<Triangle> indices = { { 0, 1, 2 } };
    std::vector<Vec3f> positions = { Vec3f(1, 0, 0), Vec3f(0, 1, 0), Vec3f(1, 1, 0) };
    std::vector<Vec3f> normals;
    return std::make_unique<TriangleMesh>(std::move(indices), std::move(positions), std::move(normals));
}

std::unique_ptr<TriangleMesh> TriangleMesh::loadFromFile(const std::string_view filename)
{
    if (!fileExists(filename)) {
        std::cout << "Could not find mesh file: " << filename << std::endl;
        return nullptr;
    }

    //TODO(Mathijs): move this out of the triangle class because it should also load material information
    // which will be stored in a SceneObject kinda way.
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filename.data(), aiProcessPreset_TargetRealtime_MaxQuality);

    if (scene == nullptr || scene->mRootNode == nullptr || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE) {
        std::cout << "Failed to load mesh file: " << filename << std::endl;
        return nullptr;
    }

    std::vector<Triangle> indices;
    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;

    std::stack<std::tuple<aiNode*, Mat3x4f>> stack;
    stack.push({ scene->mRootNode, assimpMatrix(scene->mRootNode->mTransformation) });
    while (!stack.empty()) {
        auto [node, transform] = stack.top();
        stack.pop();

        transform *= assimpMatrix(node->mTransformation);

        for (unsigned i = 0; i < node->mNumMeshes; i++) {
            // Process subMesh
            addSubMesh(scene, node->mMeshes[i], transform, indices, positions, normals);
        }

        for (unsigned i = 0; i < node->mNumChildren; i++) {
            stack.push({ node->mChildren[i], transform });
        }
    }

    if (indices.size() == 0 || positions.size() == 0)
        return nullptr;

    return std::make_unique<TriangleMesh>(std::move(indices), std::move(positions), std::move(normals));
}

unsigned TriangleMesh::numPrimitives()
{
    return m_numPrimitives;
}

gsl::span<const Bounds3f> TriangleMesh::getPrimitivesBounds()
{
    return gsl::span<const Bounds3f>(m_primitiveBounds);
}

Bounds3f TriangleMesh::getPrimitiveBounds(unsigned primitiveIndex)
{
    return m_primitiveBounds[primitiveIndex];
}

Vec3f TriangleMesh::getNormal(unsigned primitiveIndex, Vec2f uv)
{
    return m_normals[primitiveIndex];
}

bool TriangleMesh::intersect(unsigned primitiveIndex, Ray& ray)
{
    // Based on PBRT v3 triangle intersection test:
    // https://github.com/mmp/pbrt-v3/blob/master/src/shapes/triangle.cpp
    //
    // Transform the ray and triangle such that the ray origin is at (0,0,0) and its
    // direction points along the +Z axis. This makes the intersection test easy and
    // allows for watertight intersection testing.
    Triangle triangle = m_indices[primitiveIndex];
    Vec3f p0 = m_positions[triangle.i0];
    Vec3f p1 = m_positions[triangle.i1];
    Vec3f p2 = m_positions[triangle.i2];

    // Translate vertices based on ray origin
    Vec3f p0t = p0 - ray.origin;
    Vec3f p1t = p1 - ray.origin;
    Vec3f p2t = p2 - ray.origin;

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
    Vec3f d = permute(ray.direction, kx, ky, kz);
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
    ray.uv = Vec2f(b0, b1);

    return true;

    // Code that we used in the OpenCL path tracer (from Jacco's slides)
    /*// Find vectors for two edges sharing v0
    Vec3f e0 = v1 - v0;
    Vec3f e1 = v2 - v0;

    // Begin calculating determinant - also used to calculate u parameter
    Vec3f P = cross(ray.direction, e1);

    // If determinant is near zero, ray lies in plane of triangle or ray is parallel to plane of triangle
    float det = dot(e0, P);
    if (det <= std::numeric_limits<float>::min())// If det == 0.0f
        return false;
    float detReciprocal = 1.0f / det;

    // Calculate the distance from v0 to the ray origin
    Vec3f T = ray.origin - v0;

    // Calculate u parameter and test bounds
    float u = dot(T, P) * detReciprocal;
    if (u < 0.0f || u > 1.0f)
        return false;

    // Prepare to test v parameter
    Vec3f Q = cross(T, e0);

    // Calculate v parameter and test bounds
    float v = dot(ray.direction, Q) * detReciprocal;
    if (v < 0.0f || v > 1.0f)
        return false;

    // Test if the found intersection is in front of the ray (and not behind) and test if it is better
    // than the currently found hit.
    float t = dot(e1, Q) * detReciprocal;
    if (t > 0.0f && t < ray.t)
    {
        ray.t = t;
        ray.uv = Vec2f(u, v);
        return true;
    }

    return false;*/
}
}