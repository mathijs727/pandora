#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <fstream>
#include <string>
#include <string_view>

class PandoraMeshBatch
{
public:
    PandoraMeshBatch(std::string filename);
    ~PandoraMeshBatch();

    pybind11::tuple addTriangleMesh(
        pybind11::array_t<uint32_t> npTriangles,
        pybind11::array_t<float> npPositions,
        pybind11::array_t<float> npNormals,
        pybind11::array_t<float> npTangents,
        pybind11::array_t<float> npUVCoords,
        pybind11::array_t<float> transform);
private:
    size_t m_currentPos = 0;
    std::string m_filename;
    std::ofstream m_file;
};
