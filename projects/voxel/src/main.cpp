#include "pandora/geometry/triangle.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/svo/sparse_voxel_dag.h"
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

    plyFile.write(outStream, true); // write ascii
    fb.close();
}

int main()
{
    const std::string projectBasePath = "../../"s;
    auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/stanford/dragon_vrip.ply");
    //auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj");

    Bounds gridBounds;
    for (const auto& mesh : meshes)
        gridBounds.extend(mesh->getBounds());

    VoxelGrid voxelGrid(128);
    using clock = std::chrono::high_resolution_clock;
    {
        auto start = clock::now();
        for (const auto& mesh : meshes) {
            meshToVoxelGrid(voxelGrid, gridBounds, *mesh);
        }
        auto end = clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Time to voxelize: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
    }

    auto svoConstructionStart = clock::now();
    //SparseVoxelOctree svo(voxelGrid);
	SparseVoxelDAG svo(voxelGrid);
    auto svoConstructionEnd = clock::now();
    auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(svoConstructionEnd - svoConstructionStart);
    std::cout << "Time to construct SVO: " << timeDelta.count() / 1000.0f << "ms" << std::endl;

    //auto [vertices, triangles] = voxelGrid.generateSurfaceMesh();
    auto [vertices, triangles] = svo.generateSurfaceMesh();
    exportMesh(vertices, triangles, "hello_world.ply");

    std::cout << "INPUT CHARACTER AND PRESS ENTER TO EXIT:" << std::endl;
	char x;
	std::cin >> x;
    return 0;
}
