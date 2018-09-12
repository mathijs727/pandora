//#include "mesh_wrapper.h"
#include "string_to_number.h"
#include "mesh_exporter.h"
#include <boost/python.hpp>
#include <iostream>

BOOST_PYTHON_MODULE(pandora_py)
{
    using namespace boost::python;
    Py_Initialize();
    numpy::initialize();
    def("string_to_numpy_float", stringToNumpy<float>);
    def("string_to_numpy_int", stringToNumpy<int>);

    def("export_triangle_mesh", exportTriangleMesh);

    /*class_<MeshWrapper>("TriangleMesh", init<std::string>())
        .def(init<np::ndarray, np::ndarray, np::ndarray, np::ndarray, np::ndarray>()) // Second constructor
        .def("saveToCacheFile", &MeshWrapper::saveToCacheFile);*/
}
