#include <array>
#include <tinyply.h>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
#include <string_view>
#include <fstream>
#include <string>

using namespace std::string_literals;
using namespace tinyply;

void exportMesh(gsl::span<glm::vec3> vertices, gsl::span<glm::ivec3> triangles, std::string_view filePath)
{
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
    std::array vertices = { glm::vec3(-2, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 2, 0) };
    std::array indices = { glm::ivec3(0, 1, 2) };
	
	exportMesh(vertices, indices, "hello_world.ply");

    std::cout << "HELLO WORLD!" << std::endl;
    return 0;
}
