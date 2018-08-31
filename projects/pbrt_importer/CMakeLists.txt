# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)

project("python_hello_world")

find_package(Python3 COMPONENTS Interpreter Development)
find_package(Boost COMPONENTS python36)
 
# Build our library
Python3_add_library(pandora_py MODULE pandora_py.cpp)
set_target_properties(pandora_py PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF)

# Define the wrapper library that wraps our library
target_link_libraries(pandora_py PRIVATE Boost::python36 Python3::Python)

add_custom_command(
    TARGET pandora_py
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:pandora_py>"
        "${CMAKE_CURRENT_LIST_DIR}/python/$<TARGET_FILE_NAME:pandora_py>")
