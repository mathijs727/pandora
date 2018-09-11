#pragma once
#include "mesh_wrapper.h"
#include <boost/python/numpy.hpp>
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
//#include <pandora/geometry/triangle.h>
#include <string>
#include <string_view>
#include <hello_world.h>

namespace python = boost::python;
namespace np = boost::python::numpy;

/*template <typename T>
static std::unique_ptr<T[]> numpyToArray(np::ndarray npArray)
{
    int64_t elementSize = npArray.get_dtype().get_itemsize();
    int64_t numBytes = npArray.shape(0) * elementSize;
    assert(numBytes % sizeof(T) == 0);

    int64_t numElements = numBytes / sizeof(T);
    auto ret = std::make_unique<T[]>(numElements);
    const char* data = npArray.get_data();
    std::memcpy(
        reinterpret_cast<void*>(ret.get()),
        reinterpret_cast<const void*>(data),
        numBytes);
    return std::move(ret);
}

static std::shared_ptr<pandora::TriangleMesh> createTriangleMesh(
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
{
    unsigned numTriangles = (unsigned)(npTriangles.shape(0) / sizeof(glm::ivec3));
    unsigned numVertices = (unsigned)(npPositions.shape(0) / sizeof(glm::vec3));

    auto triangles = numpyToArray<glm::ivec3>(npTriangles);
    auto positions = numpyToArray<glm::vec3>(npPositions);

    std::unique_ptr<glm::vec3[]> normals = nullptr;
    if (npNormals.shape(0) != 0)
        normals = numpyToArray<glm::vec3>(npNormals);

    std::unique_ptr<glm::vec3[]> tangents = nullptr;
    if (npTangents.shape(0) != 0)
        tangents = numpyToArray<glm::vec3>(npTangents);

    std::unique_ptr<glm::vec2[]> uvCoords = nullptr;
    if (npUVCoords.shape(0) != 0)
        uvCoords = numpyToArray<glm::vec2>(npUVCoords);

    return std::make_shared<pandora::TriangleMesh>(
        numTriangles, numVertices,
        std::move(triangles),
        std::move(positions),
        std::move(normals),
        std::move(tangents),
        std::move(uvCoords));
}

static std::shared_ptr<pandora::TriangleMesh> createTriangleMesh(std::string filename)
{
    std::cout << "CREATE MESH FROM FILE" << std::endl;
    auto meshOpt = pandora::TriangleMesh::loadFromFileSingleMesh(filename);
    if (meshOpt) {
        return std::make_shared<pandora::TriangleMesh>(std::move(*meshOpt));
    } else {
        std::cout << "FAILED TO LOAD MESH FROM FILE \"" << filename << "\"!" << std::endl;
        return std::shared_ptr<pandora::TriangleMesh>(nullptr);
    }
}*/

/*MeshWrapper::MeshWrapper(
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords)
    : m_mesh(createTriangleMesh(npTriangles, npPositions, npNormals, npTangents, npUVCoords))
{
}

MeshWrapper::MeshWrapper(std::string filename)
    : m_mesh(createTriangleMesh(filename))
{
}*/

void MeshWrapper::saveToCacheFile(std::string filename)
{
    //m_mesh->saveToCacheFile(filename);
    helloWorld();
}
