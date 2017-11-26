cmake_minimum_required(VERSION 3.8)
project(dist_path_tracer)

# Output to a folder called bin to prevent the following error:
# cannot open output file pandora: Is a directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/GSL/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/GLEW/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/glfw/)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/external/googletest/)

include(libPandora/CMakeLists.txt)
include(pandora/CMakeLists.txt)