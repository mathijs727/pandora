#include "mesh_exporter.h"
#include <cassert>
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <iostream>
//#include <tinyply.h>
#include <algorithm>
#include <pandora/core/transform.h>
#include <pandora/geometry/triangle.h>
#include <pandora/utility/error_handling.h>
#include <vector>

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

PandoraMeshBatch::PandoraMeshBatch(std::string filename)
    : m_filename(filename)
    , m_file(filename)
    , m_currentPos(0)
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
    np::ndarray npUVCoords,
    boost::python::list pythonTransform)
{
    // NOTE: convert back to 32 bit numbers because assimp requires a recompile to support doubles.
    // Using 32 bit floats everywhere is also not an option because ujson and rapidjson (Python bindings) cannot serialize them.
    auto triangles = reinterpretNumpyArray<glm::i64vec3>(npTriangles);
    auto positions = reinterpretNumpyArray<glm::dvec3>(npPositions);
    auto normals = reinterpretNumpyArray<glm::dvec3>(npNormals);
    auto tangents = reinterpretNumpyArray<glm::dvec3>(npTangents);
    auto uvCoords = reinterpretNumpyArray<glm::dvec2>(npUVCoords);

    glm::mat4 transformMatrix;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            transformMatrix[i][j] = py::extract<double>(pythonTransform[j][i]);
        }
    }
    pandora::Transform transform(transformMatrix);

    pandora::ALWAYS_ASSERT(triangles.size() < std::numeric_limits<unsigned>::max());
    pandora::ALWAYS_ASSERT(positions.size() < std::numeric_limits<unsigned>::max());

    std::vector<glm::ivec3> outTriangles;
    std::copy(std::begin(triangles), std::end(triangles), std::back_inserter(outTriangles));
    std::vector<glm::vec3> outPositions;
    std::transform(std::begin(positions), std::end(positions), std::back_inserter(outPositions),
        [&](auto p) {
            return transform.transformPoint(p);
        });

    std::vector<glm::vec3> outNormals;
    if (!normals.empty()) {
        pandora::ALWAYS_ASSERT(normals.size() == triangles.size());
        std::transform(std::begin(normals), std::end(normals), std::back_inserter(outNormals),
         [&](auto n) {
            return transform.transformNormal(n);
        });
    }

    std::vector<glm::vec3> outTangents;
    if (!tangents.empty()) {
        pandora::ALWAYS_ASSERT(tangents.size() == triangles.size());
        std::transform(std::begin(tangents), std::end(tangents), std::back_inserter(outTangents),
            [&](auto t) {
            return transform.transformNormal(t); // Is this the correct transform?
        });
    }

    std::vector<glm::vec2> outUVCoords;
    if (!uvCoords.empty()) {
        pandora::ALWAYS_ASSERT(uvCoords.size() == triangles.size());
        std::copy(std::begin(uvCoords), std::end(uvCoords), std::back_inserter(outUVCoords));
    }

    pandora::TriangleMesh pandoraMesh = pandora::TriangleMesh(
        std::move(outTriangles),
        std::move(outPositions),
        std::move(outNormals),
        std::move(outTangents),
        std::move(outUVCoords));

    flatbuffers::FlatBufferBuilder fbb;
    auto serializedMesh = pandoraMesh.serialize(fbb);
    fbb.Finish(serializedMesh);

    m_file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    size_t startByte = m_currentPos;
    m_currentPos += fbb.GetSize();

    return py::make_tuple(m_filename, startByte, fbb.GetSize());
}
