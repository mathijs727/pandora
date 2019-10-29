#pragma once
#include "pandora/graphics_core/transform.h"
#include "pandora/shapes/triangle.h"
#include "pandora/utility/math.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/glm.hpp>

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

SurfaceInteraction pandora::TriangleShape::fillSurfaceInteraction(const Ray& ray, const RayHit& rayHit) const
{

    const float b0 = 1 - rayHit.geometricUV.x - rayHit.geometricUV.y;
    const float b1 = rayHit.geometricUV.x;
    const float b2 = rayHit.geometricUV.y;

    // Interpolate (u, v) parametric coordinates and hit point
    glm::vec3 p[3];
    getPositions(rayHit.primitiveID, p);
    glm::vec3 hitPos = b0 * p[0] + b1 * p[1] + b2 * p[2];
    SurfaceInteraction si { hitPos, rayHit.geometricNormal, rayHit.geometricUV, -ray.direction };

    glm::vec3 ns { si.normal };
    if (!m_normals.empty()) {
        glm::vec3 vertexNormals[3];
        getShadingNormals(rayHit.primitiveID, vertexNormals);
        ns = glm::normalize(b0 * vertexNormals[0] + b1 * vertexNormals[1] + b2 * vertexNormals[2]);
        if (glm::dot(ns, ray.direction) > 0)
            ns = -ns;
    }

    glm::vec2 st { 0 };
    if (!m_texCoords.empty()) {
        glm::vec2 vertexSt[3];
        getTexCoords(rayHit.primitiveID, vertexSt);
        st = b0 * vertexSt[0] + b1 * vertexSt[1] + b2 * vertexSt[2];
    }

    si.setShadingGeometry(ns, st);
    return si;
}

void TriangleShape::getTexCoords(unsigned primitiveID, gsl::span<glm::vec2, 3> texCoord) const
{
    glm::ivec3 indices = m_indices[primitiveID];
    texCoord[0] = m_texCoords[indices[0]];
    texCoord[1] = m_texCoords[indices[1]];
    texCoord[2] = m_texCoords[indices[2]];
}

void TriangleShape::getPositions(unsigned primitiveID, gsl::span<glm::vec3, 3> p) const
{
    glm::ivec3 indices = m_indices[primitiveID];
    p[0] = m_positions[indices[0]];
    p[1] = m_positions[indices[1]];
    p[2] = m_positions[indices[2]];
}

void TriangleShape::getShadingNormals(unsigned primitiveID, gsl::span<glm::vec3, 3> ns) const
{
    glm::ivec3 indices = m_indices[primitiveID];
    ns[0] = m_normals[indices[0]];
    ns[1] = m_normals[indices[1]];
    ns[2] = m_normals[indices[2]];
}

}