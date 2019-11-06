unset(CMAKE_TOOLCHAIN_FILE)

cmake_minimum_required(VERSION 3.10)
project(pandora)

option(PANDORA_ISPC_SUPPORT "Use ISPC to accelerate voxelization" OFF)
option(PANDORA_BUILD_TESTS "Build tests for Pandora" OFF)
option(PANDORA_BUILD_ATLAS "Build Atlas (real-time viewer)" OFF)
option(PANDORA_BUILD_VOXEL_EXPORTER "Build voxel exporter" OFF)
option(PANDORA_BUILD_PBRT_IMPORTER "Build voxel exporter" OFF)
option(PANDORA_ISA_AVX512 "Compile code that uses AVX512" OFF)
option(LINK_STDCXX_FILESYSTEM "Add libstdc++ filesystem to the linker flags" OFF)
option(LINK_CXX_FILESYSTEM "Add libc++ filesystem to the linker flags" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/enable_simd.cmake")
if (PANDORA_ISPC_SUPPORT)
	include("${CMAKE_CURRENT_LIST_DIR}/cmake/ispc.cmake")
endif()

#include("cmake/pmm.cmake")
#pmm(VERBOSE
#	DEBUG
#	VCPKG
#		REVISION ea79220f9d9d33a11101c8dea0827aaff5a2b9b3
#		REQUIRES eastl ms-gsl tbb fmt spdlog gtest assimp glm openimageio mio flatbuffers "boost-program-options" nlohmann-json embree3 libmorton
#		PORTS "${CMAKE_CURRENT_LIST_DIR}/third_party/assimp5/") # TODO: assimp version in vcpkg@ea79220f9d9d33a11101c8dea0827aaff5a2b9b3 is broken. Update 5.0.0 is available but requires VCPKG revision update = recompile everything

find_package(eastl REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(mio CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(TBB COMPONENTS tbb tbbmalloc REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenImageIO REQUIRED)
find_package(Embree3 REQUIRED)
find_package(vectorclass REQUIRED)
#find_package(hpx REQUIRED)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/third_party/optick/")

# WARNING: temporary hack because using the assimp::assimp target exported by assimp will try to link against a DLL (and fail)
add_library(assimp_target INTERFACE IMPORTED)
target_include_directories(assimp_target INTERFACE ${ASSIMP_INCLUDE_DIRS})
target_link_directories(assimp_target INTERFACE ${ASSIMP_LIBRARY_DIRS})
target_link_libraries(assimp_target INTERFACE ${ASSIMP_LIBRARIES})

#if (PANDORA_BUILD_VOXEL_EXPORTER)
#	add_library(tinyply "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/tinyply.cpp")
#	target_include_directories(tinyply PUBLIC "${CMAKE_CURRENT_LIST_DIR}/third_party/tinyply/source/")
#
#	add_subdirectory("projects/voxel/")
#endif()

find_package(Flatbuffers REQUIRED)
if (TARGET flatbuffers::flatc)
	get_property(FLATBUFFERS_FLATC_EXECUTABLE TARGET flatbuffers::flatc PROPERTY IMPORTED_LOCATION_RELEASE)
elseif (NOT DEFINED FLATBUFFERS_FLATC_EXECUTABLE)
	message(FATAL_ERROR "Please define FLATBUFFERS_FLATC_EXECUTABLE as the path to the flatbuffer executable")
endif()
include("cmake/BuildFlatBuffers.cmake") # Include cmake script that defines the flatc compile function

if (PANDORA_BUILD_TESTS)
	enable_testing()
    find_package(GTest REQUIRED)

	set(SIMD_BUILD_TESTS ON)
	set(STREAM_BUILD_TESTS ON)
endif()

if (PANDORA_BUILD_ATLAS)
    find_package(glfw3 REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(OpenGL REQUIRED)
endif()

add_subdirectory("projects/stream_framework/")
#add_subdirectory("projects/hpx_experiments/")
include("projects/simd/CMakeLists.txt")
include("projects/metrics/CMakeLists.txt")
include("projects/pandora/CMakeLists.txt")
add_subdirectory("projects/pbrt_importer2/")
if (PANDORA_BUILD_ATLAS)
    include("projects/atlas/CMakeLists.txt")
endif()
include("projects/torque/CMakeLists.txt")


#add_subdirectory("projects/diskbench/")
#if (BUILD_VOXEL_EXPORT)
#    include("projects/voxel/CMakeLists.txt")
#endif()

#if (PANDORA_BUILD_PBRT_IMPORTER)
#	add_subdirectory("projects/pbrt_importer/")	
#endif()
