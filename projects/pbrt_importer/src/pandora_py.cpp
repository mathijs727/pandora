#include "mesh_wrapper.h"
#include "string_to_number.h"
#include <boost/python.hpp>

BOOST_PYTHON_MODULE(pandora_py)
{
    using namespace boost::python;
    Py_Initialize();
    numpy::initialize();
    def("string_to_numpy_float", stringToNumpy<float>);
    def("string_to_numpy_int", stringToNumpy<int>);

    class_<MeshWrapper>("MeshWrapper") //, init<std::string>())
        //.def(init<np::ndarray, np::ndarray, np::ndarray, np::ndarray, np::ndarray>()) // Second constructor
        .def("saveToCacheFile", &MeshWrapper::saveToCacheFile);
}
