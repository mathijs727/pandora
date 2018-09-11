#pragma once
#include <boost/python/numpy.hpp>
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
#include <pandora/geometry/triangle.h>
#include <string>
#include <string_view>

class MeshWrapper {
    using ndarray = boost::python::numpy::ndarray;

public:
    MeshWrapper() = default;
    MeshWrapper(
        ndarray npTriangles,
        ndarray npPositions,
        ndarray npNormals,
        ndarray npTangents,
        ndarray npUVCoords);
    MeshWrapper(std::string filename);

    void saveToCacheFile(std::string filename);

private:
    std::shared_ptr<pandora::TriangleMesh> m_mesh = nullptr;
};
