#pragma once
#include <boost/python/numpy.hpp>
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
#include <pandora/geometry/triangle.h>
#include <string>
#include <string_view>

namespace np = boost::python::numpy;

class MeshWrapper {
    using ndarray = boost::python::numpy::ndarray;

public:

    MeshWrapper() = default;
    MeshWrapper(
        np::ndarray npTriangles,
        np::ndarray npPositions,
        np::ndarray npNormals,
        np::ndarray npTangents,
        np::ndarray npUVCoords);
    MeshWrapper(std::string filename);

    void saveToCacheFile(std::string filename);

private:
    std::shared_ptr<pandora::TriangleMesh> m_mesh = nullptr;
};
