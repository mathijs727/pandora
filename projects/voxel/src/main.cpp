#include "pandora/geometry/triangle.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/svo/sparse_voxel_octree.h"
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

const std::string projectBasePath = "../../"s;
const int resolution = 128;

using SVO = SparseVoxelDAG;
void exportMesh(gsl::span<glm::vec3> vertices, gsl::span<glm::ivec3> triangles, std::string_view filePath);
SVO createSVO(std::string_view meshFile);

int main()
{
    const std::string projectBasePath = "../../"s;
    std::string dragonFile = projectBasePath + "assets/3dmodels/stanford/dragon_vrip.ply";
    std::string bunnyFile = projectBasePath + "assets/3dmodels/stanford/bun_zipper.ply";
    std::string cornellBoxFile = projectBasePath + "assets/3dmodels/cornell_box.obj";

    if (std::is_same_v<SVO, SparseVoxelDAG>) {
        std::vector<std::pair<std::string, std::string>> files = {
            { dragonFile, "dag_dragon.ply" },
            { bunnyFile, "dag_bunny.ply" },
            { cornellBoxFile, "dag_cornell.ply" }
        };

        std::vector<SparseVoxelDAG> DAGs;
        for (const auto& [filePath, canonicalName] : files) {
            (void)canonicalName;
            DAGs.emplace_back(std::move(createSVO(filePath)));
        }

		using clock = std::chrono::high_resolution_clock;
		auto start = clock::now();
        
		compressDAGs(DAGs);

		auto end = clock::now();
		auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "Time to compress SVOs to DAGs: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
        std::cout << "Combined size after compression: " << DAGs[0].size() << " bytes" << std::endl;

        for (size_t i = 0; i < files.size(); i++) {
            auto [vertices, triangles] = DAGs[i].generateSurfaceMesh();
            exportMesh(vertices, triangles, files[i].second);
        }
    }

    std::cout << "INPUT CHARACTER AND PRESS ENTER TO EXIT:" << std::endl;
    char x;
    std::cin >> x;
    return 0;
}

SVO createSVO(std::string_view meshFile)
{
    auto meshes = TriangleMesh::loadFromFile(meshFile);

    Bounds gridBounds;
    for (const auto& mesh : meshes)
        gridBounds.extend(mesh->getBounds());

    VoxelGrid voxelGrid(resolution);
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
    SVO svo(voxelGrid);
    auto svoConstructionEnd = clock::now();
    auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(svoConstructionEnd - svoConstructionStart);
    std::cout << "Time to construct SVO: " << timeDelta.count() / 1000.0f << "ms\n"
              << std::endl;

    return std::move(svo);
}

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
