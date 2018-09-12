#pragma once
#include <boost/python/numpy.hpp>
#include <string>

namespace np = boost::python::numpy;
using ndarray = boost::python::numpy::ndarray;

void exportTriangleMesh(
    std::string filename,
    np::ndarray npTriangles,
    np::ndarray npPositions,
    np::ndarray npNormals,
    np::ndarray npTangents,
    np::ndarray npUVCoords);