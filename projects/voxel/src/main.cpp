#include "pandora/geometry/triangle.h"
#include "pandora/svo/sparse_voxel_dag.h"
#include "pandora/svo/sparse_voxel_octree.h"
#include "pandora/svo/voxel_grid.h"
#include "pandora/graphics_core/transform.h"
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <glm/glm.hpp>
#include <span>
#include <iostream>
#include <string>
#include <string_view>
#include <tinyply.h>

using namespace std::string_literals;
using namespace tinyply;
using namespace pandora;

const std::string projectBasePath = "../../../../"s;
const int resolution = 128;

using SVO = SparseVoxelDAG;
void exportMesh(std::span<glm::vec3> vertices, std::span<glm::ivec3> triangles, std::string_view filePath);
SVO createSVO(std::string_view meshFile);
void testDAGCompressionTogether(const std::span<std::pair<std::string, std::string>> files);
void testDAGCompressionSeparate(const std::span<std::pair<std::string, std::string>> files);

int main()
{
    std::string dragonFile = projectBasePath + "assets/3dmodels/stanford/dragon_vrip.ply";
    std::string bunnyFile = projectBasePath + "assets/3dmodels/stanford/bun_zipper.ply";
    std::string cornellBoxFile = projectBasePath + "assets/3dmodels/cornell_box.obj";

    if (std::is_same_v<SVO, SparseVoxelDAG>) {
        std::vector<std::pair<std::string, std::string>> files = {
            { dragonFile, "dag_dragon.ply" },
            { bunnyFile, "dag_bunny.ply" },
            { cornellBoxFile, "dag_cornell.ply" }
        };

        testDAGCompressionTogether(files);
    }

    /*auto bunnySVDAG = createSVO(bunnyFile);
    auto[positions, triangles] = bunnySVDAG.generateSurfaceMesh();
    exportMesh(positions, triangles, "mesh.ply");*/

    std::cout << "INPUT CHARACTER AND PRESS ENTER TO EXIT:" << std::endl;
    char x;
    std::cin >> x;
    return 0;
}

void testDAGCompressionTogether(const std::span<std::pair<std::string, std::string>> files)
{
    std::vector<SparseVoxelDAG> DAGs;
    for (const auto& [filePath, outFile] : files) {
        std::cout << "Model: " << outFile << std::endl;
        DAGs.emplace_back(std::move(createSVO(filePath)));
    }

    {
        size_t sizeBeforeCompression = 0;
        for (const auto& svdag : DAGs) {
            sizeBeforeCompression += svdag.sizeBytes();
        }
        std::cout << "Combined size before compression: " << sizeBeforeCompression << " bytes" << std::endl;
    }

    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();

    std::vector<SparseVoxelDAG*> dagPointers;
    for (auto& svdag : DAGs) {
        dagPointers.push_back(&svdag);
    }
    SparseVoxelDAG::compressDAGs(dagPointers);

    auto end = clock::now();
    auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "\nTime to compress SVOs to DAGs: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
    std::cout << "Combined size after compression: " << DAGs[0].sizeBytes() << " bytes" << std::endl;

    for (int i = 0; i < files.size(); i++) {
        auto [vertices, triangles] = DAGs[i].generateSurfaceMesh();
        exportMesh(vertices, triangles, files[i].second);
    }
}

void testDAGCompressionSeparate(const std::span<std::pair<std::string, std::string>> files)
{
    for (const auto& [filePath, outFile] : files) {
        std::vector<SparseVoxelDAG> DAGs;
        std::cout << "Model: " << outFile << std::endl;
        DAGs.emplace_back(std::move(createSVO(filePath)));

        using clock = std::chrono::high_resolution_clock;
        auto start = clock::now();

        std::cout << "Size before compression: " << DAGs[0].sizeBytes() << " bytes" << std::endl;

        std::vector<SparseVoxelDAG*> dagPointers;
        for (auto& svdag : DAGs) {
            dagPointers.push_back(&svdag);
        }
        SparseVoxelDAG::compressDAGs(dagPointers);

        auto end = clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Time to compress SVOs to DAGs: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
        std::cout << "Size after compression: " << DAGs[0].sizeBytes() << " bytes\n"
                  << std::endl;

        auto [vertices, triangles] = DAGs[0].generateSurfaceMesh();
        exportMesh(vertices, triangles, outFile);
    }
}

SVO createSVO(std::string_view meshFile)
{
    auto meshes = TriangleMesh::loadFromFile(meshFile);

    Bounds gridBounds;
    for (const auto& mesh : meshes)
        gridBounds.extend(mesh.getBounds());

    VoxelGrid voxelGrid(resolution);
    using clock = std::chrono::high_resolution_clock;
    {
        auto start = clock::now();
        for (const auto& mesh : meshes) {
            //meshToVoxelGrid(voxelGrid, gridBounds, mesh);
            mesh.voxelize(voxelGrid, gridBounds, Transform(glm::mat4(1.0f)));
        }
        auto end = clock::now();
        auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Time to voxelize: " << timeDelta.count() / 1000.0f << "ms" << std::endl;
    }

    auto svoConstructionStart = clock::now();
    SVO svo(voxelGrid);
    auto svoConstructionEnd = clock::now();
    auto timeDelta = std::chrono::duration_cast<std::chrono::microseconds>(svoConstructionEnd - svoConstructionStart);
    std::cout << "Time to construct SVO: " << timeDelta.count() / 1000.0f << "ms" << std::endl;

    return std::move(svo);
}

void exportMesh(std::span<glm::vec3> vertices, std::span<glm::ivec3> triangles, std::string_view filePath)
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

    plyFile.write(outStream, true);
    fb.close();
}
