#include "mesh_exporter.h"
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
#include <cassert>
//#include <tinyply.h>
#include <algorithm>
#include <vector>
#include <pandora/geometry/triangle.h>
#include <pandora/utility/error_handling.h>

namespace py = boost::python;
namespace np = boost::python::numpy;
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
    m_file(filename),
    m_currentPos(0)
{
    std::cout << "PandoraMeshBatch(" << filename << ")" << std::endl;
}

PandoraMeshBatch::~PandoraMeshBatch()
{
    std::cout << "~PandoraMeshBatch() => " << m_filename << std::endl;
}

py::object PandoraMeshBatch::addTriangleMesh(
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
{
    // NOTE: convert back to 32 bit numbers because assimp requires a recompile to support doubles.
    // Using 32 bit floats everywhere is also not an option because ujson and rapidjson (Python bindings) cannot serialize them.
    auto triangles = reinterpretNumpyArray<glm::i64vec3>(npTriangles);
    auto positions = reinterpretNumpyArray<glm::dvec3>(npPositions);
    auto normals = reinterpretNumpyArray<glm::dvec3>(npNormals);
    auto tangents = reinterpretNumpyArray<glm::dvec3>(npTangents);
    auto uvCoords = reinterpretNumpyArray<glm::dvec2>(npUVCoords);

    pandora::ALWAYS_ASSERT(triangles.size() < std::numeric_limits<unsigned>::max());
    pandora::ALWAYS_ASSERT(positions.size() < std::numeric_limits<unsigned>::max());
    unsigned numTriangles = static_cast<unsigned>(triangles.size());
    unsigned numVertices = static_cast<unsigned>(positions.size());

    auto owningTriangles = std::make_unique<glm::ivec3[]>(numTriangles);
    std::copy(std::begin(triangles), std::end(triangles), owningTriangles.get());
    auto owningPositions = std::make_unique<glm::vec3[]>(numVertices);
    std::copy(std::begin(positions), std::end(positions), std::next(owningPositions.get(), 0));

    std::unique_ptr<glm::vec3[]> owningNormals = nullptr;
    if (!normals.empty()) {
        pandora::ALWAYS_ASSERT(normals.size() == triangles.size());
        owningNormals = std::make_unique<glm::vec3[]>(numTriangles);
        std::copy(std::begin(normals), std::end(normals), std::next(owningNormals.get(), 0));
    }

    std::unique_ptr<glm::vec3[]> owningTangents = nullptr;
    if (!tangents.empty()) {
        pandora::ALWAYS_ASSERT(tangents.size() == triangles.size());
        owningTangents = std::make_unique<glm::vec3[]>(numTriangles);
        std::copy(std::begin(tangents), std::end(tangents), std::next(owningTangents.get(), 0));
    }

    std::unique_ptr<glm::vec2[]> owningUVCoords = nullptr;
    if (!uvCoords.empty()) {
        pandora::ALWAYS_ASSERT(uvCoords.size() == triangles.size());
        owningUVCoords = std::make_unique<glm::vec2[]>(numTriangles);
        std::copy(std::begin(uvCoords), std::end(uvCoords), std::next(owningUVCoords.get(), 0));
    }

    pandora::TriangleMesh pandoraMesh = pandora::TriangleMesh(numTriangles, numVertices,
        std::move(owningTriangles), std::move(owningPositions), std::move(owningNormals), std::move(owningTangents), std::move(owningUVCoords));

    flatbuffers::FlatBufferBuilder fbb;
    auto serializedMesh = pandoraMesh.serialize(fbb);
    fbb.Finish(serializedMesh);

    m_file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    size_t startByte = m_currentPos;
    m_currentPos += fbb.GetSize();

    return py::make_tuple(m_filename, startByte, fbb.GetSize());
}
