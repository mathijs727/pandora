# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)

project("pandora_python")

find_package(Python3 REQUIRED COMPONENTS Development)
find_package(pybind11 REQUIRED)

# Build our library
Python3_add_library(pandora_py
    "src/pandora_py.cpp"
    "src/mesh_exporter.cpp"
    "src/mesh_exporter.h"
    "src/string_to_number.cpp"
    "src/string_to_number.h"
	"src/crack_atof.cpp"
	"src/crack_atof.h")
target_compile_features(pandora_py PRIVATE cxx_std_17)

# Define the wrapper library that wraps our library
target_link_libraries(pandora_py PRIVATE
	pybind11::embed
	pybind11::module
	TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy
    libPandora)

if (MSVC)
    set_target_properties(pandora_py PROPERTIES COMPILE_FLAGS "/arch:AVX2")
else()
    set_target_properties(pandora_py PROPERTIES COMPILE_FLAGS "-mavx2")
endif()

add_custom_command(
    TARGET pandora_py
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:pandora_py>"
        "${CMAKE_CURRENT_LIST_DIR}/python/$<TARGET_FILE_NAME:pandora_py>")


message("Python3_VERSION: ${Python3_VERSION}")

#find_package(TBB COMPONENTS tbb tbbmalloc REQUIRED)
add_executable(benchmark "benchmark.cpp" "src/crack_atof.cpp")
target_link_libraries(benchmark PRIVATE TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
target_compile_features(benchmark PRIVATE cxx_std_17)
