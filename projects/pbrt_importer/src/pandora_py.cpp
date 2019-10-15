#include "mesh_exporter.h"
#include "string_to_number.h"
#include <gsl/span>
#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <vector>

PYBIND11_MODULE(pandora_py, m)
{
    m.doc() = "PBRT to Pandora scene converter"; // optional module docstring

    m.def("string_to_numpy_float", stringToNumpy<float>);
    m.def("string_to_numpy_double", stringToNumpy<double>);
    m.def("string_to_numpy_int32", stringToNumpy<int32_t>);
    m.def("string_to_numpy_int64", stringToNumpy<int64_t>);
    m.def("string_to_numpy_uint32", stringToNumpy<uint32_t>);
    m.def("string_to_numpy_uint64", stringToNumpy<uint64_t>);

	pybind11::class_<PandoraMeshBatch>(m, "PandoraMeshBatch")
        .def(pybind11::init<std::string>())
        .def("addTriangleMesh", &PandoraMeshBatch::addTriangleMesh);
}
