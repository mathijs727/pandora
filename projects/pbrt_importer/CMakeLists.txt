# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.12)

project("python_hello_world")

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
find_package(Boost REQUIRED COMPONENTS python36 numpy36) # CMake / Boost doesnt support Python 3.7 yet
 
# Build our library
Python3_add_library(pandora_py MODULE
    "src/pandora_py.cpp"
    "src/mesh_wrapper.cpp"
    "src/mesh_wrapper.h"
    "src/string_to_number.cpp"
    "src/string_to_number.h")
set_target_properties(pandora_py PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF)

#add_library(xxx "src2/hello_world.cpp" "src2/hello_world.h")
#target_include_directories(xxx PUBLIC "src2/")
#target_link_libraries(xxx PRIVATE TBB::tbb)

# Define the wrapper library that wraps our library
target_link_libraries(pandora_py PRIVATE
    Boost::python36
    Boost::numpy36
    libPandora)

#add_custom_command(
#    TARGET pandora_py
#    POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy
#        "$<TARGET_FILE:pandora_py>"
#        "${CMAKE_CURRENT_LIST_DIR}/python/$<TARGET_FILE_NAME:pandora_py>")
