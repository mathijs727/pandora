#pragma once
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <string>

template <typename T>
pybind11::array_t<T> stringToNumpy(std::string_view string);
