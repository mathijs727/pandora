cmake_minimum_required(VERSION 3.8)
project(pandora)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")

option(BUILD_TESTS "Build tests for Pandora" OFF)
option(BUILD_ATLAS "Build Atlas (real-time viewer)" ON)

#find_package(GSL REQUIRED)# Guideline Support Library (like the Microsoft implementation)
find_package(glm REQUIRED)

find_package(OpenImageIO REQUIRED)
add_library(OpenImageIO INTERFACE)
target_include_directories(OpenImageIO INTERFACE "${OIIO_INCLUDE_DIR}")
target_link_libraries(OpenImageIO INTERFACE "${OIIO_OpenImageIO_LIBRARY}")

find_package(assimp REQUIRED)
add_library(assimp INTERFACE)
target_include_directories(assimp INTERFACE "${ASSIMP_INCLUDE_DIRS}")
target_link_libraries(assimp INTERFACE "${ASSIMP_LIBRARIES}")

find_package(embree 3 REQUIRED)
add_library(embree INTERFACE)
target_include_directories(embree INTERFACE "${EMBREE_INCLUDE_DIRS}")
target_link_libraries(embree INTERFACE "${EMBREE_LIBRARY}")

find_package(TBB REQUIRED COMPONENTS tbbmalloc)
add_library(tbb INTERFACE)
target_include_directories(tbb INTERFACE "${TBB_INCLUDE_DIRS}")
target_link_libraries(tbb INTERFACE "${TBB_LIBRARIES}")

if (BUILD_TESTS)
    find_package(GTest REQUIRED)
endif ()

if (BUILD_ATLAS)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

include("projects/pandora/CMakeLists.txt")
if (BUILD_ATLAS)
    include("projects/atlas/CMakeLists.txt")
endif()
#include("torque/CMakeLists.txt")
