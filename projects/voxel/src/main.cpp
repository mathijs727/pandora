#include <array>
#include <tinyply.h>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
#include <string_view>
#include <fstream>
#include <string>
#include <cassert>
#include "pandora/svo/voxel_grid.h"
#include "pandora/svo/mesh_to_voxel.h"
#include "pandora/geometry/triangle.h"

using namespace std::string_literals;
using namespace tinyply;
using namespace pandora;

void exportMesh(gsl::span<glm::vec3> vertices, gsl::span<glm::ivec3> triangles, std::string_view filePath)
{
	assert(vertices.size() > 0 && triangles.size() > 0);

	std::filebuf fb;
	fb.open(std::string(filePath), std::ios::out | std::ios::binary);
	std::ostream outStream(&fb);
	if (outStream.fail()) throw std::runtime_error("failed to open "s + std::string(filePath));

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
	auto meshes = TriangleMesh::loadFromFile(projectBasePath + "assets/3dmodels/cornell_box.obj");
	
	Bounds gridBounds;
	for (const auto& mesh : meshes)
		gridBounds.extend(mesh->getBounds());

	int resolution = 20;
	VoxelGrid voxelGrid(resolution);
	//for (const auto& mesh : meshes)
	//	meshToVoxelGridNaive(voxelGrid, gridBounds, *mesh, resolution);
	for (int i = 0; i < 1; i++) {
		meshToVoxelGridNaive(voxelGrid, gridBounds, *meshes[i], resolution);
	}

	auto [vertices, triangles] = voxelGrid.generateSurfaceMesh();
	exportMesh(vertices, triangles, "hello_world.ply");

    std::cout << "HELLO WORLD!" << std::endl;
    return 0;
}
