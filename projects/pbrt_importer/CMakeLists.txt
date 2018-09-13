# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)

project("pandora_python")

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(Boost REQUIRED COMPONENTS python37 numpy37)
 
# Build our library
Python3_add_library(pandora_py MODULE
    "src/pandora_py.cpp"
    "src/mesh_exporter.cpp"
    "src/mesh_exporter.h"
    #"src/mesh_wrapper.cpp"
    #"src/mesh_wrapper.h"
    "src/string_to_number.cpp"
    "src/string_to_number.h")
set_target_properties(pandora_py PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF)

# Define the wrapper library that wraps our library
target_link_libraries(pandora_py PRIVATE
    Boost::python37
    Boost::numpy37
    Python3::Python
    #libPandora
    #tinyply
    glm)

add_custom_command(
    TARGET pandora_py
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:pandora_py>"
        "${CMAKE_CURRENT_LIST_DIR}/python/$<TARGET_FILE_NAME:pandora_py>")
