#include "mesh_exporter.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <gsl/gsl>
#include <pandora/graphics_core/transform.h>
#include <pandora/shapes/triangle.h>
#include <pandora/utility/error_handling.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>
#include <chrono>

template <typename Tout, typename Tin>
static gsl::span<const Tout> reinterpretNumpyArray(pybind11::array_t<Tin> npArray)
{
    const int64_t elementSize = npArray.itemsize();
    const int64_t numBytes = npArray.shape(0) * elementSize;
    pandora::ALWAYS_ASSERT(numBytes % sizeof(Tout) == 0);
    const int64_t numElements = numBytes / sizeof(Tout);

    if (numElements == 0)
        return {};

    const Tin* data = npArray.data();
    return gsl::span(reinterpret_cast<const Tout*>(data), numElements);
}

template <typename T, typename S>
static std::vector<T> castArray(gsl::span<const S> in)
{
    std::vector<T> ret { in.size() };
    std::transform(std::begin(in), std::end(in), std::begin(ret), [](const S& v) { return static_cast<T>(v); });
    return ret;
}

PandoraMeshBatch::PandoraMeshBatch(std::string filename)
    : m_filename(filename)
    , m_file(filename)
    , m_currentPos(0)
{
    spdlog::info("PandoraMeshBatch::PandoraMeshBatch({})", filename);
    pandora::ALWAYS_ASSERT(std::filesystem::exists(m_filename));
}

PandoraMeshBatch::~PandoraMeshBatch()
{
    spdlog::info("PandoraMeshBatch::~PandoraMeshBatch()");
}

pybind11::tuple PandoraMeshBatch::addTriangleMesh(
    pybind11::array_t<uint32_t> npTriangles,
    pybind11::array_t<float> npPositions,
    pybind11::array_t<float> npNormals,
    pybind11::array_t<float> npTangents,
    pybind11::array_t<float> npUVCoords,
    pybind11::array_t<float> pythonTransform)
{
    // NOTE: convert back to 32 bit numbers because assimp requires a recompile to support doubles.
    // Using 32 bit floats everywhere is also not an option because ujson and rapidjson (Python bindings) cannot serialize them.
    auto triangles = reinterpretNumpyArray<glm::uvec3>(npTriangles);
    auto positions = reinterpretNumpyArray<glm::vec3>(npPositions);
    auto normals = reinterpretNumpyArray<glm::vec3>(npNormals);
    auto tangents = reinterpretNumpyArray<glm::vec3>(npTangents);
    auto texCoords = reinterpretNumpyArray<glm::vec2>(npUVCoords);

    pandora::ALWAYS_ASSERT(pythonTransform.ndim() == 2);
    pandora::ALWAYS_ASSERT(pythonTransform.shape(0) == 4);
    pandora::ALWAYS_ASSERT(pythonTransform.shape(1) == 4);
    glm::mat4 transformMatrix;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            //transformMatrix[i][j] = py::extract<double>(pythonTransform[j][i]);
            transformMatrix[i][j] = pythonTransform.at(i, j);
        }
    }
    pandora::Transform transform(transformMatrix);

    pandora::ALWAYS_ASSERT(triangles.size() < std::numeric_limits<unsigned>::max());
    pandora::ALWAYS_ASSERT(positions.size() < std::numeric_limits<unsigned>::max());

    std::vector<glm::uvec3> outTriangles;
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

    std::vector<glm::vec2> outUVCoords;
    if (!texCoords.empty()) {
        pandora::ALWAYS_ASSERT(texCoords.size() == positions.size());
        std::copy(std::begin(texCoords), std::end(texCoords), std::back_inserter(outUVCoords));
    }

    pandora::TriangleShape pandoraShape = pandora::TriangleShape(
        std::move(outTriangles),
        std::move(outPositions),
        std::move(outNormals),
        std::move(outUVCoords));

    /*flatbuffers::FlatBufferBuilder fbb;
    auto serializedMesh = pandoraShape.serialize();
    fbb.Finish(serializedMesh);

    //spdlog::info("Write to file {}", m_filename);
    m_file.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    size_t startByte = m_currentPos;
    m_currentPos += fbb.GetSize();

    return pybind11::make_tuple(m_filename, startByte, fbb.GetSize());*/
}
