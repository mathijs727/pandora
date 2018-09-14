#include "mesh_exporter.h"
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
//#include <tinyply.h>
#include <algorithm>
#include <vector>

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

PandoraMeshBatch::PandoraMeshBatch(std::string filename) :
    m_filename(filename),
    m_file(filename)
{
}

boost::python::tuple PandoraMeshBatch::addTriangleMesh(
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

    size_t startByte = m_file.tellp();

    m_file << "o PandoraMesh\n";
    for (auto p : positions)
        m_file << "v " << p.x << " " << p.y << " " << p.z << "\n";

    int numChannels = 1;
    if (!normals.empty()) {
        for (auto n : normals)
            m_file << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        numChannels++;
    }

    if (!uvCoords.empty()) {
        for (auto uv : uvCoords)
            m_file << "vt " << uv.x << " " << uv.y << "\n";
        numChannels++;
    }

    for (auto t : triangles) {
        m_file << "f";
        for (int i = 0; i < 3; i++) {
            auto index = t[i] + 1;// OBJ starts counting at 1...
            m_file << " " << index;
            for (int c = 1; c < numChannels; c++) {
                m_file << "/" << index;
            }
        }
        m_file << "\n";
    }

    size_t sizeBytes = static_cast<size_t>(m_file.tellp()) - startByte;
    return boost::python::make_tuple(m_filename, startByte, sizeBytes);
}
