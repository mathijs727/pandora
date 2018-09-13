#include "mesh_exporter.h"
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
//#include <tinyply.h>
#include <vector>
#include <algorithm>

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

template <typename T, typename S>
static std::vector<T> castArray(gsl::span<S> in)
{
    std::vector<T> ret(in.size());
    std::transform(std::begin(in), std::end(in), std::begin(ret), [](const S& v) { return static_cast<T>(v); });
    return ret;
}

void exportTriangleMesh(
    std::string filename,
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
{
    // NOTE: convert back to 32 bit numbers because assimp requires a recompile to support doubles.
    // Using 32 bit floats everywhere is also not an option because ujson and rapidjson (Python bindings) cannot serialize them.
    auto triangles = reinterpretNumpyArray<glm::ivec3>(npTriangles);
    auto positions = reinterpretNumpyArray<glm::dvec3>(npPositions);
    auto normals = reinterpretNumpyArray<glm::dvec3>(npNormals);
    auto tangents = reinterpretNumpyArray<glm::dvec3>(npTangents);
    auto uvCoords = reinterpretNumpyArray<glm::dvec2>(npUVCoords);

    std::ofstream file;
    file.open(filename);
    file << "o PandoraMesh\n";

    for (auto p : positions)
        file << "v " << p.x << " " << p.y << " " << p.z << "\n";


    int numChannels = 1;
    if (!normals.empty()) {
        for (auto n : normals)
            file << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        numChannels++;
    }

    if (!uvCoords.empty()) {
        for (auto uv : uvCoords)
            file << "vt " << uv.x << " " << uv.y << "\n";
        numChannels++;
    }

    for (auto t : triangles)
    {
        file << "f";
        for (int i = 0; i < 3; i++) {
            file << " " << (t[i] + 1);
            for (int c = 1; c < numChannels; c++) {
                file << "/" << t[i];
            }
        }
        file << "\n";
    }

    /*
 f.write("o PandoraMesh\n")

positions = geometry["P"]["value"]
for p0, p1, p2 in zip(*[positions[x::3] for x in (0, 1, 2)]):
    f.write(f"v {p0} {p1} {p2}\n")

if "N" in geometry and "uv" in geometry:
    normals = geometry["N"]["value"]
    for n0, n1, n2 in zip(*[normals[x::3] for x in (0, 1, 2)]):
        f.write(f"vn {n0} {n1} {n2}\n")

    uv_coords = geometry["uv"]["value"]
    for uv0, uv1 in zip(*[uv_coords[x::2] for x in (0, 1)]):
        f.write(f"vt {uv0} {uv1}\n")

    indices = geometry["indices"]["value"]
    for i0, i1, i2 in zip(*[indices[x::3] for x in (0, 1, 2)]):
        # OBJ starts counting at 1...
        f.write(
            f"f {i0+1}/{i0+1}/{i0+1} {i1+1}/{i1+1}/{i1+1} {i2+1}/{i2+1}/{i2+1}\n")
else:
    indices = geometry["indices"]["value"]
    for i0, i1, i2 in zip(*[indices[x::3] for x in (0, 1, 2)]):
        # OBJ starts counting at 1...
        f.write(f"f {i0+1} {i1+1} {i2+1}\n")
*/

    file.close();
}

/*
void exportTriangleMesh(
    std::string filename,
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
{
    // NOTE: convert back to 32 bit numbers because assimp requires a recompile to support doubles.
    // Using 32 bit floats everywhere is also not an option because ujson and rapidjson (Python bindings) cannot serialize them.
    auto triangles = reinterpretNumpyArray<glm::ivec3>(npTriangles);
    auto positions = castArray<glm::vec3>(reinterpretNumpyArray<glm::dvec3>(npPositions));
    auto normals = castArray<glm::vec3>(reinterpretNumpyArray<glm::dvec3>(npNormals));
    auto tangents = castArray<glm::vec3>(reinterpretNumpyArray<glm::dvec3>(npTangents));
    auto uvCoords = castArray<glm::vec2>(reinterpretNumpyArray<glm::dvec2>(npUVCoords));

    tinyply::PlyFile plyFile;
    plyFile.add_properties_to_element("face", { "vertex_indices" },
        tinyply::Type::INT32, triangles.size(), reinterpret_cast<uint8_t*>(triangles.data()), tinyply::Type::UINT8, 3);
    plyFile.add_properties_to_element("vertex", { "x", "y", "z" },
        tinyply::Type::FLOAT32, positions.size(), reinterpret_cast<uint8_t*>(positions.data()), tinyply::Type::INVALID, 0);

    if (!normals.empty()) {
        plyFile.add_properties_to_element("vertex", { "nx", "ny", "nz" },
            tinyply::Type::FLOAT32, normals.size(), reinterpret_cast<uint8_t*>(normals.data()), tinyply::Type::INVALID, 0);
    }

    // Tangent vectors in ply files are not supported
    //plyFile.add_properties_to_element("vertex", { "sx", "sy", "sz" }, // What is the correct name here???
    //    tinyply::Type::FLOAT32, tangents.size(), reinterpret_cast<uint8_t*>(tangents.data()), tinyply::Type::INVALID, 0);

    if (!uvCoords.empty()) {
        plyFile.add_properties_to_element("vertex", { "u", "v" },
            tinyply::Type::FLOAT32, uvCoords.size(), reinterpret_cast<uint8_t*>(uvCoords.data()), tinyply::Type::INVALID, 0);
    }

    std::filebuf fb;
    fb.open(filename, std::ios::out | std::ios::binary);
    std::ostream outStream(&fb);
    if (outStream.fail())
        std::cerr << "Failed to open \"" << filename << "\" for writing" << std::endl;

    plyFile.write(outStream, false); // Write binary
    fb.close();
}
*/