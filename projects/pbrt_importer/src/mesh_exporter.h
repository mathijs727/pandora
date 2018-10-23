#pragma once
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <fstream>
#include <string>
#include <string_view>

using ndarray = boost::python::numpy::ndarray;

class PandoraMeshBatch
{
public:
    PandoraMeshBatch(std::string filename);
    ~PandoraMeshBatch() = default;

    boost::python::object addTriangleMesh(
        ndarray npTriangles,
        ndarray npPositions,
        ndarray npNormals,
        ndarray npTangents,
        ndarray npUVCoords);
private:
    std::string m_filename;
    std::ofstream m_file;
};
