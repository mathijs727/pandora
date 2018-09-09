#pragma once
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>
#include <boost/python/str.hpp>
#include <string>

template <typename T>
boost::python::object stringToNumpy(boost::python::str string);