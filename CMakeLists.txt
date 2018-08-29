cmake_minimum_required(VERSION 3.8)
project(pandora)

option(PANDORA_ISPC_SUPPORT "Use ISPC to accelerate voxelization" OFF)
option(PANDORA_BUILD_TESTS "Build tests for Pandora" OFF)
option(PANDORA_BUILD_ATLAS "Build Atlas (real-time viewer)" ON)
option(PANDORA_BUILD_VOXEL_EXPORTER "Build voxel exporter" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")
if (PANDORA_ISPC_SUPPORT)
	include("${CMAKE_CURRENT_LIST_DIR}/cmake/ispc.cmake")
endif()

#find_package(GSL REQUIRED)# Guideline Support Library (like the Microsoft implementation)
find_package(glm REQUIRED)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/eastl" EXCLUDE_FROM_ALL)

find_package(OpenImageIO REQUIRED)
add_library(OpenImageIO INTERFACE)
target_include_directories(OpenImageIO INTERFACE "${OIIO_INCLUDE_DIR}")
target_link_libraries(OpenImageIO INTERFACE "${OIIO_OpenImageIO_LIBRARY}")

find_package(assimp REQUIRED)
add_library(assimp_target INTERFACE)
target_include_directories(assimp_target INTERFACE "${ASSIMP_INCLUDE_DIRS}")
target_link_libraries(assimp_target INTERFACE "${ASSIMP_LIBRARIES}")

find_package(embree 3 REQUIRED)
add_library(embree INTERFACE)
target_include_directories(embree INTERFACE "${EMBREE_INCLUDE_DIRS}")
target_link_libraries(embree INTERFACE "${EMBREE_LIBRARY}")

find_package(Threads REQUIRED)
find_package(TBB REQUIRED COMPONENTS tbbmalloc)
add_library(tbb INTERFACE)
target_include_directories(tbb INTERFACE "${TBB_INCLUDE_DIRS}")
target_link_libraries(tbb INTERFACE "${TBB_LIBRARIES}" Threads::Threads)

# https://github.com/mandreyel/mio
add_library(mio INTERFACE)
target_include_directories(mio INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/mio/include/")

# https://github.com/Forceflow/libmorton
add_library(libmorton INTERFACE)
target_include_directories(libmorton INTERFACE "${CMAKE_CURRENT_LIST_DIR}/third_party/libmorton/libmorton/include/")

find_package(Boost REQUIRED)

find_package(nlohmann_json)

if (PANDORA_BUILD_TESTS)
    find_package(GTest REQUIRED)
endif()

if (PANDORA_BUILD_ATLAS)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

if (BUILD_VOXEL_EXPORTER)
	add_library(tinyply "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/tinyply.cpp")
	target_include_directories(tinyply PUBLIC "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/")
endif()

if (PANDORA_BUILD_TESTS)
	#set(SIMD_BUILD_TESTS ON)
endif()
include("projects/simd/CMakeLists.txt")
include("projects/metrics/CMakeLists.txt")
include("projects/pandora/CMakeLists.txt")
if (PANDORA_BUILD_ATLAS)
    include("projects/atlas/CMakeLists.txt")
endif()
include("projects/torque/CMakeLists.txt")
if (BUILD_VOXEL_EXPORT)
	include("projects/voxel/CMakeLists.txt")
endif()


#find_package(prometheus-cpp)
#include("projects/prometheus_test/CMakeLists.txt")


