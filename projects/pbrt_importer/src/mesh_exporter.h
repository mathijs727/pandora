#pragma once
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <fstream>
#include <string>
#include <string_view>

namespace np = boost::python::numpy;
using ndarray = boost::python::numpy::ndarray;

class PandoraMeshBatch
{
public:
    PandoraMeshBatch(std::string filename);
    ~PandoraMeshBatch() = default;

    boost::python::tuple addTriangleMesh(
        np::ndarray npTriangles,
        np::ndarray npPositions,
        np::ndarray npNormals,
        np::ndarray npTangents,
        np::ndarray npUVCoords);
private:
    std::string m_filename;
    std::ofstream m_file;
};
