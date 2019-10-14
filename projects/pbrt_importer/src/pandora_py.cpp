//#include "mesh_exporter.h"
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
    m.def("string_to_numpy_int", stringToNumpy<int>);
}


/*//#include "mesh_wrapper.h"
#include "mesh_exporter.h"
#include "string_to_number.h"
#include <boost/python.hpp>
#include <iostream>

BOOST_PYTHON_MODULE(pandora_py)
{
    using namespace boost::python;
    Py_Initialize();
    numpy::initialize();
    def("string_to_numpy_float", stringToNumpy<float>);
    def("string_to_numpy_double", stringToNumpy<double>);
    def("string_to_numpy_int", stringToNumpy<int>);

    //def("export_triangle_mesh", exportTriangleMesh);

    class_<PandoraMeshBatch, boost::noncopyable>("PandoraMeshBatch", init<std::string>())
        .def("addTriangleMesh", &PandoraMeshBatch::addTriangleMesh);
}*/
