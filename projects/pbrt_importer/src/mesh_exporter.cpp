#include "mesh_exporter.h"
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
#include <tinyply.h>

using namespace std::string_literals;

template <typename T>
static gsl::span<T> reinterpretNumpyArray(np::ndarray npArray)
{
    int64_t elementSize = npArray.get_dtype().get_itemsize();
    int64_t numBytes = npArray.shape(0) * elementSize;
    assert(numBytes % sizeof(T) == 0);
    int64_t numElements = numBytes / sizeof(T);

    if (numElements == 0)
        return {};

    auto* data = npArray.get_data();
    return gsl::make_span(reinterpret_cast<T*>(data), numElements);
}

void exportTriangleMesh(
    std::string filename,
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
{
    std::cout << "EXPORT" << std::endl;
    auto triangles = reinterpretNumpyArray<glm::ivec3>(npTriangles);
    auto positions = reinterpretNumpyArray<glm::vec3>(npPositions);
    auto normals = reinterpretNumpyArray<glm::vec3>(npNormals);
    auto tangents = reinterpretNumpyArray<glm::vec3>(npTangents);
    auto uvCoords = reinterpretNumpyArray<glm::vec2>(npUVCoords);

    tinyply::PlyFile plyFile;
    plyFile.add_properties_to_element("face", { "vertex_indices" },
        tinyply::Type::INT32, triangles.size(), reinterpret_cast<uint8_t*>(triangles.data()), tinyply::Type::UINT8, 3);
    plyFile.add_properties_to_element("vertex", { "x", "y", "z" },
        tinyply::Type::FLOAT32, positions.size(), reinterpret_cast<uint8_t*>(positions.data()), tinyply::Type::INVALID, 0);

    std::cout << "Faces:" << std::endl;
    for (auto face : triangles)
        std::cout << "(" << face.x << ", " << face.y << ", " << face.z << ")" << std::endl;

    std::cout << "Positions:" << std::endl;
    for (auto v : positions)
        std::cout << "(" << v.x << ", " << v.y << ", " << v.z << ")" << std::endl;

    if (!normals.empty()) {
        std::cout << "HAS NORMALS" << std::endl;
        plyFile.add_properties_to_element("vertex", { "nx", "ny", "nz" },
            tinyply::Type::FLOAT32, normals.size(), reinterpret_cast<uint8_t*>(normals.data()), tinyply::Type::INVALID, 0);
    }

    // Tangent vectors in ply files are not supported
    //plyFile.add_properties_to_element("vertex", { "sx", "sy", "sz" }, // What is the correct name here???
    //    tinyply::Type::INT32, tangents.size(), reinterpret_cast<uint8_t*>(tangents.data()), tinyply::Type::INVALID, 0);

    if (!uvCoords.empty()) {
        std::cout << "HAS UV COORDS" << std::endl;
        plyFile.add_properties_to_element("vertex", { "u", "v" },
            tinyply::Type::FLOAT32, uvCoords.size(), reinterpret_cast<uint8_t*>(uvCoords.data()), tinyply::Type::INVALID, 0);
    }

    std::filebuf fb;
    fb.open(filename, std::ios::out | std::ios::binary);
    std::ostream outStream(&fb);
    if (outStream.fail())
        std::cerr << "Failed to open \"" << filename << "\" for writing" << std::endl;

    plyFile.write(outStream, true); // Write binary
    fb.close();
}