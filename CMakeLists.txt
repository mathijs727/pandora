cmake_minimum_required(VERSION 3.8)
project(pandora)

option(BUILD_TESTS "Build tests for Pandora" OFF)
option(BUILD_ATLAS "Build Atlas (real-time viewer)" ON)

#find_package(GSL REQUIRED)# Guideline Support Library (like the Microsoft implementation)
find_package(tbb REQUIRED)
find_package(glm REQUIRED)

find_package(assimp REQUIRED)
add_library(assimp INTERFACE)
target_include_directories(assimp INTERFACE "${ASSIMP_INCLUDE_DIRS}")
target_link_libraries(assimp INTERFACE "${ASSIMP_LIBRARIES}")

find_package(embree 3.0 REQUIRED)
add_library(embree INTERFACE)
target_include_directories(embree INTERFACE "${EMBREE_INCLUDE_DIRS}")
target_link_libraries(embree INTERFACE "${EMBREE_LIBRARY}")
message("ASSIMP LIB NAME: ${ASSIMP_LIBRARIES}")

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