#include <gsl/span>
#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <vector>

int add(int i, int j)
{
    return i + j;
}

int sum(pybind11::array_t<int> data)
{
    std::cout << "HELLO WORLD" << std::endl;
    return data.at(0);
}

template <typename T>
pybind11::array_t<T> toNumpyArray(gsl::span<const T> items)
{
    pybind11::array_t<T> outArray { static_cast<pybind11::size_t>(items.size()) };
    for (int i = 0; i < items.size(); i++)
        outArray.mutable_at(i) = items[i];
    return outArray;
}

pybind11::array_t<int> createArray()
{
    std::vector<int> items = { 1, 2, 3, 4, 5 };
    return toNumpyArray<int>(items);
}

PYBIND11_MODULE(pandora_py, m)
{
    m.doc() = "pybind11 example plugin"; // optional module docstring

    m.def("add", &add, "A function which adds two numbers");
    m.def("sum", &sum);
    m.def("create_array", &createArray, pybind11::return_value_policy::move);
}

/*

//#include "mesh_wrapper.h"
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
}


*/