#include "mesh_to_voxel_ispc.h"
#include "pandora/geometry/triangle.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/svo/voxel_grid.h"
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
#include <string>
#include <string_view>
#include <tinyply.h>

using namespace std::string_literals;
using namespace tinyply;
using namespace pandora;

void exportMesh(gsl::span<glm::vec3> vertices, gsl::span<glm::ivec3> triangles, std::string_view filePath)
{
    assert(vertices.size() > 0 && triangles.size() > 0);

    std::filebuf fb;
    fb.open(std::string(filePath), std::ios::out | std::ios::binary);
    std::ostream outStream(&fb);
    if (outStream.fail())
        throw std::runtime_error("failed to open "s + std::string(filePath));

    PlyFile plyFile;
    plyFile.add_properties_to_element("vertex", { "x", "y", "z" },
        Type::FLOAT32, vertices.size(), reinterpret_cast<uint8_t*>(vertices.data()), Type::INVALID, 0);
    plyFile.add_properties_to_element("face", { "vertex_indices" },
        Type::INT32, triangles.size(), reinterpret_cast<uint8_t*>(triangles.data()), Type::UINT8, 3);

    plyFile.write(outStream, false); // write ascii
    fb.close();
}

int main()
{
    const std::string projectBasePath = "../../"s;
    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/bun_zipper.ply");
    //auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj");

    Bounds gridBounds;
    for (const auto& mesh : meshes)
        gridBounds.extend(mesh->getBounds());

    int resolution = 64;
    VoxelGrid voxelGrid(resolution);

    ispc::Bounds ispcGridBounds;
    ispcGridBounds.min.v[0] = gridBounds.min.x;
    ispcGridBounds.min.v[1] = gridBounds.min.y;
    ispcGridBounds.min.v[2] = gridBounds.min.z;

    ispcGridBounds.max.v[0] = gridBounds.max.x;
    ispcGridBounds.max.v[1] = gridBounds.max.y;
    ispcGridBounds.max.v[2] = gridBounds.max.z;

    for (const auto& mesh : meshes) {
        //const auto& mesh = meshes[2];
        const auto& triangles = mesh->getTriangles();
        const auto& positions = mesh->getPositions();

        std::vector<ispc::int32_t3> ispcTriangles(triangles.size());
        std::transform(std::begin(triangles), std::end(triangles), std::begin(ispcTriangles), [](const glm::ivec3& t) {
            ispc::int32_t3 result;
            result.v[0] = t[0];
            result.v[1] = t[1];
            result.v[2] = t[2];
            return result;
        });

        std::vector<ispc::float3> ispcPositions(positions.size());
        std::transform(std::begin(positions), std::end(positions), std::begin(ispcPositions), [](const glm::vec3& p) {
            ispc::float3 result;
            result.v[0] = p.x;
            result.v[1] = p.y;
            result.v[2] = p.z;
            return result;
        });

        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();
        ispc::meshToVoxelGrid(voxelGrid.data(), voxelGrid.resolution(), ispcGridBounds, ispcPositions.data(), ispcTriangles.data(), triangles.size());
        //meshToVoxelGridNaive(voxelGrid, gridBounds, *mesh);
        auto end = clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Time to voxelize: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
    }
    /*for (const auto& mesh : meshes) {
        meshToVoxelGridNaive(voxelGrid, gridBounds, *mesh);
    }*/

    auto [vertices, triangles] = voxelGrid.generateSurfaceMesh();
    exportMesh(vertices, triangles, "hello_world.ply");

    std::cout << "HELLO WORLD!" << std::endl;
    return 0;
}
